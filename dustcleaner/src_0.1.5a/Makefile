#Makefile written by Frank

CC   = gcc
CPP  = g++


SRC = dustfinder.c utilities.c dustanalyzer.c recovery.c dustcleaner_plugin_main.c dustcleaner_UI.c
OBJ = dustfinder.o utilities.o dustanalyzer.o recovery.o dustcleaner_plugin_main.o dustcleaner_UI.o
OUTPUT = output.txt marked.jpg output1.txt
BIN  = dustcleaner
LIBS =  `pkg-config --libs opencv` `gimptool --libs`
INCS =  `pkg-config --cflags opencv` `gimptool --cflags`
RM = rm -f
CFLAGS = $(INCS) 

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LIBS)	

$(OBJ): $(SRC)
	$(CC) -c $(SRC) $(CFLAGS)	

.PHONY: clean
clean:
	${RM} $(OBJ) $(BIN) $(OUTPUT)

