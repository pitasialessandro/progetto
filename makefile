# Variabili
JAVAC = javac
SRC = *.java

all: compile

compile:
	$(JAVAC) $(SRC)

run: compile
	java CreaGrafo

clean:
	rm -f *.class
