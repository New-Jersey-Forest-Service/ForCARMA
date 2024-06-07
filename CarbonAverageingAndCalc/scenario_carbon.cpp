//
//// Automate computation of scenario carbon and total carbon
//
// (c) 2024 Greg Johnson Biometrics LLC
// 01/23/2024
// 04/15/2024 Modified program to compute cumulative effects of multiple scenarios.
// 

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <numeric>      // std::iota
#include <algorithm>    // std::sort, std::stable_sort
#include <vector>
#include <map>

struct ACRES {
    double Total;
    std::vector<double> Scenario;
};

struct CARBON {
    double TotalAcres = 0.0;
    double ScenarioAcres = 0.0;
    double NonScenarioCarbon = 0.0;
    double ScenarioCarbon = 0.0;
    double NetCarbon = 0.0;
};

typedef std::pair<int,int> KEY; // first FTG, second Year

// Usage:
// scenario_carbon acres.csv lab.csv scenario.csv foresttypegroup_definitions.csv
//
// returns  0 if successful
//         -1 if no csv file name argument given
//         -2 could not open csv file
//         -3 error reading year column
//         -4 scenarios in acreage and carbon csv files do not match
//
int main( int argc, char **argv)
{
    size_t n_scenarios = 0;

    // check to see if proper number arguments present
    if( argc < 5 )
    {
        std::cerr << "Command line requires 4 parameters, " << argc-1 << " supplied\n";
        std::cerr << "Usage:\nscenario_carbon acres.csv lab.csv scenario.csv foresttypegroup_definitions.csv\n";
        return -1;
    }

    // attempt to open csv files
    std::string acresfilename{argv[1]};
    std::ifstream acresfile{acresfilename};
    if( !acresfile.is_open() )
    {
        std::cerr << "Did not find or could not open " << acresfilename << "\n";
        return -2;
    }

    std::string labfilename{argv[2]};
    std::ifstream labfile{labfilename};
    if( !labfile.is_open() )
    {
        std::cerr << "Did not find or could not open " << labfilename << "\n";
        return -2;
    }

    std::string scenariofilename{argv[3]};
    std::ifstream scenariofile{scenariofilename};
    if( !scenariofile.is_open() )
    {
        std::cerr << "Did not find or could not open " << scenariofilename << "\n";
        return -2;
    }

    std::string ftgfilename{argv[4]};
    std::ifstream ftgfile{ftgfilename};
    if( !ftgfile.is_open() )
    {
        std::cerr << "Did not find or could not open " << ftgfilename << "\n";
        return -2;
    }

    //// read and parse csvs
    std::string line;
    int row_no = 1; 

    //////////////////////////////////////////////////////////////////////////////////
    // acres
    std::map<KEY, ACRES> acres;

    // skip header row and then read and parse file
    std::getline(acresfile, line);
    while( !acresfile.eof() ) 
    {
        std::getline(acresfile, line);
        std::istringstream ss(std::move(line));
 
        // read each column of the csv
        std::string value; 

        // FTG
        std::getline(ss, value, ',');
        KEY key;
        try {
            key.first = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading FTG on row " << row_no << "\n";
            return -3;
        }

        // Year
        std::getline(ss, value, ',');
        try {
            key.second = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr  << "Error reading Year on row " << row_no << "\n";
            return -3;
        }

        // Total_Stand_Carbon
        std::getline(ss, value, ',');
        double total = 0.0;
        try {
            total = std::atof( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading Total Acres on row " << row_no << "\n";
            return -3;
        }

        // read multiple scenario acreages
        std::vector<double> scenario;
        for( ; std::getline(ss, value, ','); )
        {
            try {
                scenario.emplace_back( std::atof( value.c_str() ) );
            } catch( const std::exception &e ) {
                std::cerr << "Error reading Scenario Acres on row " << row_no << "\n";
                return -3;
            }
        }

        ACRES row = {total, scenario};
        acres[key] = row;
        row_no++;
    }

    acresfile.close();
    std::cerr << row_no << " rows read from " << acresfilename << "\n";


    //////////////////////////////////////////////////////////////////////////////////
    // lab conditions carbon
    std::map<KEY, double> lab_conditions;
    row_no = 1;

    // skip header row and then read and parse file
    std::getline(labfile, line);
    while( !labfile.eof() ) 
    {
        std::getline(labfile, line);
        std::istringstream ss(std::move(line));
 
        // read each column of the csv
        std::string value; 

        // FTG
        std::getline(ss, value, ',');
        KEY key;
        try {
            key.first = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading FTG on row " << row_no << "\n";
            return -3;
        }

        // Year
        std::getline(ss, value, ',');
        try {
            key.second = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading Year on row " << row_no << "\n";
            return -3;
        }

        // Lab Conditions Carbon
        std::getline(ss, value, ',');
        double carbon = 0.0;
        try {
            carbon = std::atof( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading Lab Conditions carbon on row " << row_no << "\n";
            return -3;
        }

        lab_conditions[key] = carbon;
        row_no++;
    }

    labfile.close();
    std::cerr << row_no << " rows read from " << labfilename << "\n";

    //////////////////////////////////////////////////////////////////////////////////
    // scenario conditions carbon
    std::map<KEY, std::vector<double>> scenario_conditions;
    row_no = 1;

    // skip header row and then read and parse file
    std::getline(scenariofile, line);
    while( !scenariofile.eof() ) 
    {
        std::getline(scenariofile, line);
        std::istringstream ss(std::move(line));
 
        // read each column of the csv
        std::string value; 

        // FTG
        std::getline(ss, value, ',');
        KEY key;
        try {
            key.first = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading FTG on row " << row_no << "\n";
            return -3;
        }

        // Year
        std::getline(ss, value, ',');
        try {
            key.second = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading Year on row " << row_no << "\n";
            return -3;
        }

        // scenario Conditions Carbon (accommodate multiple scenarios)
        std::vector<double> carbon;
        for( ; std::getline(ss, value, ','); )
        {         
            try {
                carbon.emplace_back( std::atof( value.c_str() ) );
            } catch( const std::exception &e ) {
                std::cerr << "Error reading scenario Conditions carbon on row " << row_no << "\n";
                return -3;
            }
        }

        scenario_conditions[key] = carbon;
        row_no++;
    }

    scenariofile.close();
    std::cerr << row_no << " rows read from " << scenariofilename << "\n";
    
    //////////////////////////////////////////////////////////////////////////////////
    // Forest type group definitions
    std::map<int, std::string> ftg_definitions;
    row_no = 1;

    // skip header row and then read and parse file
    std::getline(ftgfile, line);
    while( !ftgfile.eof() ) 
    {
        std::string value;

        std::getline(ftgfile, line);
        std::istringstream ss(std::move(line));
 
        // forest type group label
        std::string forest_type_group; 
        std::getline(ss, forest_type_group, ',');

        // FTG
        std::getline(ss, value, ',');
        int ftg = 0;
        try {
            ftg = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << "Error reading Forest Type Groups on row " << row_no << "\n";
            return -3;
        }

        ftg_definitions[ftg] = forest_type_group;
        row_no++;
    }

    ftgfile.close();
    std::cerr << row_no << " rows read from " << ftgfilename << "\n";

    //////////////////////////////////////////////////////////////////////////////////

    std::map< KEY, std::vector<CARBON> > carbon;
    std::map< KEY, CARBON > accumulated_carbon;

    // accumulate carbon values
    for( auto &[key, labCarbonperAcre] : lab_conditions )
    {
        try {
            CARBON c;
            ACRES  a = acres.at(key);
            auto conditions = scenario_conditions.at(key);

            if( a.Scenario.size() != conditions.size() )
            {
                std::cerr << "Number of scenarios does not match between acres and carbon data\n";  
                return -4;  
            }

            n_scenarios = conditions.size();

            for( size_t i = 0; i < n_scenarios; i++ )
            {
                c.ScenarioCarbon = a.Scenario[i] * conditions[i];
                c.NonScenarioCarbon = (a.Total - a.Scenario[i]) * labCarbonperAcre;
                c.NetCarbon = c.ScenarioCarbon + c.NonScenarioCarbon;
                c.ScenarioAcres = a.Scenario[i];
                c.TotalAcres = a.Total;

                accumulated_carbon[key].ScenarioCarbon += a.Scenario[i] * conditions[i];
                accumulated_carbon[key].ScenarioAcres += a.Scenario[i];

                carbon[key].emplace_back( c );
            }

            accumulated_carbon[key].NonScenarioCarbon = (a.Total - accumulated_carbon[key].ScenarioAcres) * labCarbonperAcre;
            accumulated_carbon[key].NetCarbon = accumulated_carbon[key].ScenarioCarbon + accumulated_carbon[key].NonScenarioCarbon;
            accumulated_carbon[key].TotalAcres = a.Total;

        } catch( const std::exception &e ) {
            std::cerr << "Error at FTG " << key.first << ", Year " << key.second << "\n";
        }
    }

    // write out results
    std::cout << "FTG, Year, Total_Acres";
    for( size_t i = 0; i < n_scenarios; i++ )
        std::cout << ", Scenario_Acres" << i << ", Scenario_Carbon" << i << ", NonScenario_Carbon" << i << ", Net_Carbon" << i;
    std::cout << ", Accum_Scenario_Acres, Accum_Scenario_Carbon, Accumulated_Non_Scenario_Carbon, Accumulated_Net_Carbon\n";

    for( auto &[key, item] : carbon )
    {
        std::cout << key.first << ", " << key.second;

        for( size_t i = 0; i < item.size(); i++ )
        {
            if( i == 0 )
                std::cout <<  ", " << item[i].TotalAcres;

            if( item[i].TotalAcres > 0.0  )
            {
                std::cout <<  ", " <<
                        item[i].ScenarioAcres << ", " <<
                        item[i].ScenarioCarbon << ", " <<
                        item[i].NonScenarioCarbon << ", " <<
                        item[i].NetCarbon;
            }
        }

        std::cout <<  ", " <<
                accumulated_carbon[key].ScenarioAcres << ", " <<
                accumulated_carbon[key].ScenarioCarbon << ", " <<
                accumulated_carbon[key].NonScenarioCarbon << ", " <<
                accumulated_carbon[key].NetCarbon;

        std::cout << "\n";
    }


    return 0;
}