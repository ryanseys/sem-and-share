CC=g++
FILE_NAME=sem_and_share
all:
	@echo "Compiling $(FILE_NAME).cpp.."
	@$(CC) $(FILE_NAME).cpp -o $(FILE_NAME)
	@echo "Compiled $(FILE_NAME).cpp successfully!\n"
