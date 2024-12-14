# Makefile to compile and link my_program.c with pthread

CC = gcc                # Compiler to use
CFLAGS = -Wall          # Compiler flags (e.g., enable all warnings)
LDFLAGS = -pthread      # Linker flags for pthread

# Target to build the program
all: my_program

# Rule to compile and link the program
my_program: my_program.c
	$(CC) $(CFLAGS) my_program.c -o my_program $(LDFLAGS)

# Clean up generated files
clean:
	rm -f my_program
