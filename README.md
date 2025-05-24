# hexdump
 A simple terminal-based hex viewer in C, written as an exercise.

 For each file specified as an argument, a table is displayed with information about the contents of the file.
 The columns of the table are:
 the **offset**, from the start of the file, of the row of bytes that is being displayed;
 the **hexadecimal representaion** of a row of bytes;
 the **ASCII representation** of a row of bytes.

 Example usage:
 ```
 hexdump.exe my_file.txt my_other_file.png -width:12
 ```

# Building
 Just invoke any C compiler on `hexdump.c`.
 If you are on Windows, you can use the provided `build.bat` - just make sure to be in a Developer Command Prompt.
