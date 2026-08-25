# STARTUP.BAT -- runs at power-on; hold a key at the banner to skip it.
# Aliases live in RAM, so they are defined here every boot.

# the CP/M programs, each through a .SUB on A: that ends in EXIT,
# so quitting the program comes all the way back to this shell
ALIAS MBASIC CPM K-MBAS
ALIAS TURBO  CPM K-TURBO
ALIAS WS     CPM K-WS

# odds and ends
ALIAS LL DIR A
