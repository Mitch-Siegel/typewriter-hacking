import subprocess
import serial
import time
import os
import sys

import select

import re


ser = serial.Serial("/dev/ttyUSB0", 115200)

time.sleep(2)
ser.write("\r".encode())
print("Serial opened: " + ser.name)
# print("bitch asse".encode())
# ser.write("bitch asse".encode())

# from https://stackoverflow.com/questions/616645/how-to-duplicate-sys-stdout-to-a-log-file


class Logger(object):
    def __init__(self):
        self.terminal = sys.stdout

    def write(self, message):
        self.terminal.write(message)
        ser.write(message.encode())

    def flush():
        self.terminal.flush()
        ser.flush()

# sys.stdout = Logger()


"""
based off psh: a simple shell written in Python
from https://danishpraka.sh/2018/09/27/shell-in-python.html
"""


def execute_command(command):
    ser.write((command + "\n").encode())
    if(command == ''):
        return
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
            # ls has special behavior - when not using '-l' newlines are not rendered
            stripNL = ("ls" in command and (len(re.findall(r"-[^\s]*l", command)) == 0))
            p = subprocess.run(command.split(" "), capture_output=True, text=True)

            stdoutStr = "".join(p.stdout)
            stderrStr = "".join(p.stderr)
            if (stripNL):
                stdoutStr = stdoutStr.replace("\n", "  ")
                if (stdoutStr != ''):
                    stdoutStr += "\n"
                stderrStr = stderrStr.replace("\n", "  ")
                if (stderrStr != ''):
                    stderrStr += "\n"


            sys.stdout.write(stdoutStr)
            sys.stderr.write(stderrStr)
            ser.write(stdoutStr.encode())
            ser.write(stderrStr.encode())
            sys.stdout.flush()
            ser.flush()

    except PermissionError as e:
        errstr = e.strerror + "\n"
        sys.stderr.write(errstr)
        ser.write(errstr.encode())
        sys.stdout.flush()
        ser.flush()

    except FileNotFoundError as e:
        errstr = e.strerror + ": " + command + "\n"
        sys.stdout.write(errstr)
        ser.write(errstr.encode())
        sys.stdout.flush()
        ser.flush()

        # print("psh: command or file not found: {}".format(command))

    # except Exception:
    # print("psh: command not found: {}".format(command))


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
            print(cwd, end='')
        else:
            print(cwd.split('/')[-1], end='')

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
print("Serial closed: " + ser.name)
