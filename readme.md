#brainfucc
This is a brainfuck compiler I wrote in C. It compiles Brainfuck to x64 asm. Brainfuck is explained [here.](http://en.wikipedia.org/wiki/Brainfuck)

The compiler optimizes strings of increment/decrement instructions into single add/sub instructions, so for example ```+++++``` is compiled to ```addb $5, (%rdi)```.

The program allocates 30 kB at the beginning of the program, and does not dynamically reallocate if you increment the data pointer past the end, so doing too many ```>``` moves will cause a segfault.
