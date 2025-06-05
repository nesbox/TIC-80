message("Convert ${IN} to hex data...")

file(READ ${IN} FILEDATA HEX)
string(REGEX MATCHALL "([A-Za-z0-9][A-Za-z0-9])" SEPARATED_HEX ${FILEDATA})
list(JOIN SEPARATED_HEX ", 0x" DATAHEX)
string(PREPEND DATAHEX "0x")
file(WRITE ${OUT} ${DATAHEX})