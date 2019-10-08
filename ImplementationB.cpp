
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>
#include <string.h>

const static int ARRAY_SIZE = 130000;
char lines[ARRAY_SIZE][16];

#define MPI_ON
#ifdef MPI_ON
#include "mpi.h"
#endif

/************* Feel free to change code below (even main() function), add functions, etc.. But do not change CL arguments *************/

struct letter_only: std::ctype<char>
{
    letter_only(): std::ctype<char>(get_table()) {}

    static std::ctype_base::mask const* get_table()
    {
        static std::vector<std::ctype_base::mask>
                rc(std::ctype<char>::table_size,std::ctype_base::space);

        std::fill(&rc['A'], &rc['z'+1], std::ctype_base::alpha);
        return &rc[0];
    }
};

void DoOutput(std::string word, int result)
{
    std::cout << "Word Frequency: " << word << " -> " << result << std::endl;
}

int countFrequency(int max_lines, char data[], const char* word)
{
    int freq = 0;
    for (int i=0; i<max_lines; i++) {
        char line[16];
        memcpy(line, data+(i*16), sizeof(line));
        if (strcmp(word, line) == 0) {
            freq++;
        }
    }
    return freq;
}

int main(int argc, char* argv[]) {
    int processId = 0;
    int numberOfProcesses = 1;
    double start_time, end_time;

#ifdef MPI_ON
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &processId);
    MPI_Comm_size( MPI_COMM_WORLD, &numberOfProcesses);
#else
    printf("Not using MPI\n");
#endif
    //printf("array size %d\n", ARRAY_SIZE);

    if (argc != 4) {
        if (processId == 0) {
            std::cout << "ERROR: Incorrect number of arguments. Format is: <path to search file> <search word> <b1/b2>"
                      << std::endl;
        }
#ifdef MPI_ON
        MPI_Finalize();
#endif
        return 0;
    }
    std::string word = argv[2];

    memset(lines, 0, sizeof(lines));
    int words_count = 0;
    if (processId == 0) {
        std::ifstream file(argv[1]);
        file.imbue(std::locale(std::locale(), new letter_only()));
        printf("%s\n", argv[1]);
        if (!file.is_open()) {
            std::cout << "ERROR: Could not open file " << argv[1] << std::endl;
            return 0;
        }
        std::string workString;
        int i = 0;
        while (file >> workString) {
            memset(lines[i], '\0', 16);
            memcpy(lines[i], workString.c_str(), workString.length());
            //printf("line %s\n", lines[i]);
            i = i+ 1;
            words_count += 1;
        }
    }

    char buf[(ARRAY_SIZE / numberOfProcesses) * 16];

    std::string opt = argv[3];
    int using_reduction = 0;
    if( !opt.compare("b1") ) {
        if (processId == 0) {
            std::cout << "Using reduction" << std::endl;
        }
        using_reduction = 1;

    } else {
        if (processId == 0) {
            std::cout << "Using ring" << std::endl;
        }
    }
    int word_count_total = 0;

#ifdef MPI_ON
    int process_id;
    int num_procs;

    MPI_Comm_rank( MPI_COMM_WORLD, &process_id);
    MPI_Comm_size( MPI_COMM_WORLD, &num_procs);

    //send size to others
    int num_elements_per_proc = 0;
    if (process_id == 0) {
        num_elements_per_proc = (words_count + num_procs - 1) / num_procs;
        for (int r = 1; r<numberOfProcesses; r++) {
            MPI_Send(&num_elements_per_proc, 1, MPI_INT, r, 0, MPI_COMM_WORLD);
        }
    }
    else {
        MPI_Recv(&num_elements_per_proc, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    int sub_lines_size = 16 * num_elements_per_proc;
    char* sub_lines = (char*) malloc(sub_lines_size);

    // scatter image

    start_time=MPI_Wtime();

    MPI_Scatter(lines, sub_lines_size, MPI_CHAR,
                sub_lines, sub_lines_size, MPI_CHAR,
                0, MPI_COMM_WORLD);

    int word_count = countFrequency(num_elements_per_proc, (char*) sub_lines, word.c_str());

    if (using_reduction) {
        MPI_Reduce(&word_count, &word_count_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else {
        MPI_Request request = MPI_REQUEST_NULL;
        if (process_id != 1) {
            int neighbor_count;
            MPI_Irecv(&neighbor_count, 1, MPI_INT, (process_id-1) % numberOfProcesses, 0, MPI_COMM_WORLD, &request);
            MPI_Status status;
            MPI_Wait(&request, &status);
            word_count += neighbor_count;
            if (process_id ==0) {
                word_count_total = word_count;
            }
        }
        if (process_id > 0) {
            MPI_Isend(&word_count, 1, MPI_INT, (process_id+1) % numberOfProcesses, 0, MPI_COMM_WORLD, &request);
        }
    }
    if (process_id==0) {
        end_time = MPI_Wtime();
        std::cout << "MPI time: " << ((double)end_time-start_time) << std::endl;
    }

#else
    word_count_total = countFrequency(words_count, (char*) lines, word.c_str());
#endif

    if(processId == 0) {
        DoOutput(word, word_count_total);
    }
#ifdef MPI_ON
    MPI_Finalize();
#endif
    return 0;
}
