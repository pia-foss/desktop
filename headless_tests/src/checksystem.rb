require 'etc'

def check_system
    return :windows if Etc.uname[:sysname] == "Windows_NT"
    return :linux if Etc.uname[:sysname] == "Linux"
    return :macos if Etc.uname[:sysname] == "Darwin"
end
