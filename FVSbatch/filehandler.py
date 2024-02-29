# CHANGE WHAT 'N' equals to indicate how many times you want to run each keyfile
N = 5 
#DO NOT EDIT BELOW THIS LINE

import os
# function runfiles takes path to a directory as input, and calls the FVS 
# client from the command line on every keyfile in the directory
def runfiles(directory, n):
    dir_list = os.listdir(directory)
    #makes list of every keyfile in directory
    for x in dir_list: #iterates through each keyfile
        if(x.endswith(".key")): #checks for .key extension
            for y in range(n):# calls FVS 'n' times
                command ="FVSne --keywordfile=" + x
                os.system(command)
        else:
            continue

a = os.getcwd() #sets working directory to current directory



runfiles(a, N)

print("\nSuccessfully ran keyfiles")
input("press enter to exit ")