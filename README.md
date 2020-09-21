# Flex
Splits a file into x constituents or joins n files into one. Also prints the contents of specified byte range in specified file.

Flex = "F" i "l" e "Ex"cavate

Especially useful for very large files that are difficult or too big to open in editors. Works on binary and text files. To edit
contents of any file can use "rplc" utility, also one of my repos here on GitHub.

Usage:
Splits a file into x constituents or joins n files into one.
  Also prints the contents of specifed byte range in specified file.
      flex [-dsj] fileName [startByte [endByte] ] [destFile srcFile1 srcFile2 ...]
  If startByte is not given, printing starts from the first byte.
  If endByte is not given, 500 bytes are printed from startByte.
  -d Dump excavated bytes to file with .excv extension.
  -sn split fileName n times into approximately equal sizes with suffix '_x'.
  -j join files (srcFile1 srcFile2 ...) into one destination file (destFile).
