//
//// Automate computation of average total stand carbon by MgmtID and year from raw data
//
// (c) 2024 Greg Johnson Biometrics LLC
// 01/16/2024
// 

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <numeric>      // std::iota
#include <algorithm>    // std::sort, std::stable_sort
#include <vector>
#include <map>

struct ROW {
    std::string MgmtID;
    std::string StandID;
    int         Year;
    double      Total_Stand_Carbon;
};

struct AVERAGES {
    double carbon;
    int count;
};

// command line argument contains a file name of a csv containing the raw data to be averaged
// returns  0 if successful
//         -1 if no csv file name argument given
//         -2 could not open csv file
//         -3 error reading year column
//
int main( int argc, char **argv)
{
    // check to see if a csv file name supplied on command line
    if( argc < 2 )
    {
        std::cerr << "CSV file name to be processed not specified on the command line\n";
        return -1;
    }

    // attempt to open csv file
    std::string filename{argv[1]};
    std::ifstream input{filename};

    if( !input.is_open() )
    {
        std::cerr << "did not find or could not open " << filename << "\n";
        return -2;
    }

    // read and parse csv
    std::vector<ROW> csvRows;

    int row_no = 1;
    std::string line;

    // skip header row and then read and parse file
    std::getline(input, line);
    while( !input.eof() ) 
    {
        std::getline(input, line);
        std::istringstream ss(std::move(line));
        ROW row;
 
        // read each column of the csv
        std::string value; 

        // MgmtID
        std::getline(ss, row.MgmtID, ',');

        // StandID
        std::getline(ss, row.StandID, ',');

        // Year
        std::getline(ss, value, ',');
        try {
            row.Year = std::atoi( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << e.what() << "\nreading Year on row " << row_no << "\n";
            return -3;
        }

        // Total_Stand_Carbon
        std::getline(ss, value, ',');
        try {
            row.Total_Stand_Carbon = std::atof( value.c_str() );
        } catch( const std::exception &e ) {
            std::cerr << e.what() << "\nreading Total_Stand_Carbon on row " << row_no << "\n";
            return -3;
        }

        csvRows.push_back(std::move(row));
        row_no++;
    }

    input.close();
    std::cerr << row_no << " rows read from " << filename << "\n";

    std::map< std::pair<std::string,int>, AVERAGES > averages;

    // accumulate carbon values
    for( auto &row : csvRows )
    {
        std::pair<std::string,int> p( row.MgmtID, row.Year );
        averages[p].carbon += row.Total_Stand_Carbon;
        averages[p].count++;
    }

    // compute averages
    for( auto &[p, avg] : averages )
    {
        if( avg.count > 0 )
            avg.carbon /= (double)avg.count; 
    }

    // write out results
    std::cout << "MgmtID, Year, Avg_Carbon\n";
    for( auto &[id, avg] : averages )
        if( id.second > 0 )
            std::cout << id.first << ", " << id.second << ", " << avg.carbon << "\n";

    return 0;
}