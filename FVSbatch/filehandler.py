import os
# function runfiles takes path to a directory as input, and calls the FVS 
# client from the command line on every keyfile in the directory
def runfiles(directory):
    dir_list = os.listdir(directory)
    #makes list of every keyfile in directory
    for x in dir_list: #iterates through each keyfile
        if(x.endswith(".key")):
            command = "FVSne --keywordfile=" + x #delete echo before run
            os.system(command)
        else:
            continue
            
       

a = os.getcwd() #sets working directory to current directory
runfiles(a) #passes current directory to function.
