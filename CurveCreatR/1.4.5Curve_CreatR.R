# ForCARMA Carbon Analysis Scaffold Script
# Version: Scaffold v1.4.5 — Multi-scenario overlay support with composite prompt

# --- Libraries ---
library(RSQLite)
library(dplyr)
library(tidyr)
library(ggplot2)
library(readr)

# --- Helper Functions ---
extract_stand_map <- function(db_path) {
  conn <- dbConnect(SQLite(), db_path)
  raw_cases <- dbReadTable(conn, "FVS_Cases") %>%
    select(StandID, MgmtID)
  dbDisconnect(conn)
  
  duplicates <- raw_cases %>% count(StandID) %>% filter(n > 1)
  if (nrow(duplicates) > 0) {
    cat(paste0("\n[Warning] StandIDs with multiple MgmtIDs detected. Using the first occurrence for each:\n",
               paste(duplicates$StandID, collapse = ", "), "\n"))
  }
  
  stand_map <- raw_cases %>%
    group_by(StandID) %>%
    slice(1) %>%
    ungroup() %>%
    distinct()
  
  return(stand_map)
}

read_fvs_carbon <- function(db_path) {
  stand_map <- extract_stand_map(db_path)
  conn <- dbConnect(SQLite(), db_path)
  carbon <- dbReadTable(conn, "FVS_Carbon") %>%
    select(StandID, Year, Total_Stand_Carbon)
  dbDisconnect(conn)
  
  carbon <- carbon %>%
    left_join(stand_map, by = "StandID") %>%
    filter(!is.na(MgmtID)) %>%
    select(MgmtID, StandID, Year, Total_Stand_Carbon)
  
  return(carbon)
}

aggregate_carbon_coefficients <- function(df) {
  df %>%
    group_by(MgmtID, Year) %>%
    summarise(Carbon_Coefficient = mean(Total_Stand_Carbon, na.rm = TRUE), .groups = "drop")
}

apply_acreage <- function(coef_df, acres_df) {
  left_join(acres_df, coef_df, by = "MgmtID") %>%
    mutate(Net_Carbon = Carbon_Coefficient * Acres) %>%
    group_by(Year) %>%
    summarise(Total_Carbon = sum(Net_Carbon, na.rm = TRUE), .groups = "drop")
}

prompt_acreage_input <- function(mgmt_ids, scenario_label) {
  cat(paste0("\nEnter acreage for each MgmtID in scenario: ", scenario_label, "\n"))
  df <- data.frame(MgmtID = mgmt_ids, Acres = NA_real_)
  for (i in seq_len(nrow(df))) {
    val <- readline(paste0("  Acres for MgmtID ", df$MgmtID[i], ": "))
    df$Acres[i] <- as.numeric(val)
  }
  return(df)
}

combine_acreage_with_fallback <- function(scenario_acres, lab_acres) {
  full_mgmtids <- union(scenario_acres$MgmtID, lab_acres$MgmtID)
  merged <- data.frame(MgmtID = full_mgmtids) %>%
    left_join(scenario_acres, by = "MgmtID") %>%
    left_join(lab_acres, by = "MgmtID", suffix = c("_scenario", "_lab")) %>%
    mutate(Acres = ifelse(!is.na(Acres_scenario), Acres_scenario, Acres_lab)) %>%
    select(MgmtID, Acres)
  return(merged)
}

adjust_acres_by_stand_shift <- function(lab_carbon, scenario_carbon, lab_acres, scenario_mgmtid) {
  moved_stands <- scenario_carbon %>%
    select(StandID, MgmtID_scenario = MgmtID) %>%
    distinct() %>%
    left_join(lab_carbon %>% select(StandID, MgmtID_lab = MgmtID) %>% distinct(), by = "StandID") %>%
    filter(MgmtID_scenario != MgmtID_lab)
  
  lab_shrink <- moved_stands %>%
    count(MgmtID_lab, name = "Removed_Stands") %>%
    left_join(lab_acres, by = c("MgmtID_lab" = "MgmtID")) %>%
    mutate(Fraction_Removed = Removed_Stands / sum(Removed_Stands),
           Adjusted_Acres = Acres * (1 - Fraction_Removed)) %>%
    select(MgmtID = MgmtID_lab, Acres = Adjusted_Acres)
  
  reassigned_acres <- lab_acres %>% summarise(Total = sum(Acres)) %>% pull(Total) - sum(lab_shrink$Acres)
  scenario_add <- data.frame(MgmtID = scenario_mgmtid, Acres = reassigned_acres)
  
  bind_rows(lab_shrink, scenario_add)
}

build_composite_curve <- function(scenario_names, coefficients_list, acreage_list, lab_coefs, lab_acres, lab_carbon, scenario_list) {
  all_scenario_coefs <- bind_rows(coefficients_list[scenario_names]) %>%
    group_by(MgmtID, Year) %>%
    summarise(Carbon_Coefficient = first(Carbon_Coefficient), .groups = "drop")
  
  scenario_mgmtids <- unique(all_scenario_coefs$MgmtID)
  scenario_acres <- bind_rows(acreage_list[scenario_names]) %>%
    group_by(MgmtID) %>% summarise(Acres = sum(Acres), .groups = "drop")
  
  merged <- full_join(lab_coefs, all_scenario_coefs, by = c("MgmtID", "Year"), suffix = c("_lab", "_scenario")) %>%
    mutate(Carbon_Coefficient = ifelse(!is.na(Carbon_Coefficient_scenario), Carbon_Coefficient_scenario, Carbon_Coefficient_lab)) %>%
    select(MgmtID, Year, Carbon_Coefficient)
  
  adjusted_acres <- lab_acres %>%
    filter(!MgmtID %in% scenario_mgmtids) %>%
    bind_rows(scenario_acres)
  
  composite_df <- apply_acreage(merged, adjusted_acres) %>%
    mutate(Scenario = paste(scenario_names, collapse = "_plus_"))
  
  return(composite_df)
}

# --- Step 0: Load LabConditions DB ---
cat("Enter path to LabConditions FVSOut.db:\n")
lab_path <- readline("Path: ")
lab_carbon <- read_fvs_carbon(lab_path)
write_csv(extract_stand_map(lab_path), "stand_to_mgmt.csv")
cat("\nSaved: stand_to_mgmt.csv (from LabConditions)\n")

# --- Step 1: Load Additional Scenarios ---
db_paths <- c(lab_path)
labels <- c("LabConditions")

cat("\nEnter additional FVSOut.db paths for management scenarios (ENTER blank line to finish):\n")
while (TRUE) {
  path <- readline("Enter path to FVSOut.db: ")
  if (path == "") break
  db_paths <- c(db_paths, trimws(path))
  label <- readline(paste0("Label for scenario in file [", basename(path), "]: "))
  labels <- c(labels, label)
}

# --- Step 2: Load and Aggregate Data ---
scenario_list <- list()
coefficients_list <- list()
acreage_list <- list()

for (i in seq_along(db_paths)) {
  scenario_label <- labels[i]
  fvs_carbon <- read_fvs_carbon(db_paths[i])
  scenario_list[[scenario_label]] <- fvs_carbon
  
  coef_df <- aggregate_carbon_coefficients(fvs_carbon)
  coefficients_list[[scenario_label]] <- coef_df
  
  mgmt_ids <- unique(coef_df$MgmtID)
  acreage_list[[scenario_label]] <- prompt_acreage_input(mgmt_ids, scenario_label)
}

# --- Step 3: Generate Curves ---
curve_results <- list()
lab_coefs <- coefficients_list[["LabConditions"]]

for (scenario in names(coefficients_list)) {
  scenario_coefs <- coefficients_list[[scenario]]
  scenario_acres <- acreage_list[[scenario]]
  
  if (scenario == "LabConditions") {
    final_df <- apply_acreage(scenario_coefs, scenario_acres)
  } else {
    merged <- full_join(lab_coefs, scenario_coefs, by = c("MgmtID", "Year"), suffix = c("_lab", "_scenario")) %>%
      mutate(Carbon_Coefficient = ifelse(!is.na(Carbon_Coefficient_scenario), Carbon_Coefficient_scenario, Carbon_Coefficient_lab)) %>%
      select(MgmtID, Year, Carbon_Coefficient)
    
    adjusted_acres <- adjust_acres_by_stand_shift(lab_carbon, scenario_list[[scenario]], acreage_list[["LabConditions"]], scenario_acres$MgmtID[1])
    final_df <- apply_acreage(merged, adjusted_acres)
  }
  
  curve_results[[scenario]] <- final_df %>% mutate(Scenario = scenario)
}

# --- Step 4: Prompt for Composite Overlay ---
cat("\nEnter scenario names to combine into a composite (comma-separated), or leave blank to skip:\n")
composite_input <- readline("Scenarios: ")
composite_names <- strsplit(composite_input, ",")[[1]] %>% trimws()

if (length(composite_names) > 0 && all(composite_names %in% names(coefficients_list))) {
  composite_df <- build_composite_curve(
    composite_names,
    coefficients_list,
    acreage_list,
    lab_coefs,
    acreage_list[["LabConditions"]],
    lab_carbon,
    scenario_list
  )
  curve_results[[composite_df$Scenario[1]]] <- composite_df
}

# --- Step 5: Plot All Curves ---
combined_curve <- bind_rows(curve_results)

ggplot(combined_curve %>% filter(Year >= 2025), aes(x = Year, y = Total_Carbon, color = Scenario)) +
  geom_line(linewidth = 1.2) +
  scale_y_continuous(labels = scales::comma) +
  labs(
    title = "Carbon Projections by Scenario",
    y = "Total Stand Carbon (tons)",
    x = "Year",
    color = "Scenario"
  ) +
  theme_minimal()
