import subprocess
import serial
import time
import os
import sys


ser = serial.Serial("/dev/ttyUSB0", 115200)

time.sleep(2)
print("Serial opened: " + ser.name)
# print("bitch asse".encode())
# ser.write("bitch asse".encode())


# Based on https://gist.github.com/327585 by Anand Kunal, from http://www.tentech.ca/2011/05/stream-tee-in-python-saving-stdout-to-file-while-keeping-the-console-alive/
class stream_tee(object):
    def __init__(self, stream1, stream2):
        self.stream1 = stream1
        self.stream2 = stream2
        self.__missing_method_name = None # Hack!
 
    def __getattribute__(self, name):
        return object.__getattribute__(self, name)
 
    def __getattr__(self, name):
        self.__missing_method_name = name # Could also be a property
        return getattr(self, '__methodmissing__')
 
    def __methodmissing__(self, *args, **kwargs):
            # Emit method call to the log copy
            callable2 = getattr(self.stream2, self.__missing_method_name)
            callable2(*args, **kwargs)
 
            # Emit method call to stdout (stream 1)
            callable1 = getattr(self.stream1, self.__missing_method_name)
            return callable1(*args, **kwargs)



"""
based off psh: a simple shell written in Python
from https://danishpraka.sh/2018/09/27/shell-in-python.html
"""


def execute_command(command):
    ser.write((command + "\n").encode())
    """execute commands and handle piping"""
    try:
        if "|" in command:
            # save for restoring later on
            s_in, s_out = (0, 0)
            s_in = os.dup(0)
            s_out = os.dup(1)

            # first command takes commandut from stdin
            fdin = os.dup(s_in)

            # iterate over all the commands that are piped
            for cmd in command.split("|"):
                # fdin will be stdin if it's the first iteration
                # and the readable end of the pipe if not.
                os.dup2(fdin, 0)
                os.close(fdin)

                # restore stdout if this is the last command
                if cmd == command.split("|")[-1]:
                    fdout = os.dup(s_out)
                else:
                    fdin, fdout = os.pipe()

                # redirect stdout to pipe
                os.dup2(fdout, 1)
                os.close(fdout)

                try:
                    subprocess.run(cmd.strip().split())
                except Exception:
                    print("psh: command not found: {}".format(cmd.strip()))

            # restore stdout and stdin
            os.dup2(s_in, 0)
            os.dup2(s_out, 1)
            os.close(s_in)
            os.close(s_out)
        else:
            subprocess.run(command.split(" "))
            
    except Exception:
        print("psh: command not found: {}".format(command))


def psh_cd(path):
    """convert to absolute path and change directory"""
    try:
        os.chdir(os.path.abspath(path))
    except Exception:
        print("cd: no such file or directory: {}".format(path))


def psh_help():
    print("""psh: shell implementation in Python.
          Supports all basic shell commands.""")


def main():
    while True:
        cwd = os.getcwd()
        if (cwd == '/'):
            printedcwd = cwd
        else:
            printedcwd = cwd.split('/')[-1]

        print(printedcwd, end = '')
        ser.write((printedcwd + " $ ").encode())

        inp = input(" $ ")
        if inp == "exit":
            break
        elif inp[:3] == "cd ":
            psh_cd(inp[3:])
        elif inp == "help":
            psh_help()
        else:
            execute_command(inp)


if '__main__' == __name__:
    main()

ser.flush()
ser.close()
print("Serial closed: " + ser.name);
