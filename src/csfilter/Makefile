CLASSES  = \
	Block.java CallRet.java CFG.java CFGconstruct.java Filter.java SetException.java test.java

JFLAGS = -classpath .
JARFLAGS = cvfm

JAVAC    = javac
JAR      = jar

jars: classes
	$(JAR) $(JARFLAGS) csfilter.jar Filter.MF *.class

.SUFFIXES: .java .class

.java.class:
	$(JAVAC) $(JFLAGS) $*.java

default: classes

classes: $(CLASSES:.java=.class)

clean:
	find -name '*.class' |xargs rm -rf
	rm csfilter.jar

install:
	cp csfilter.jar ../../bin
