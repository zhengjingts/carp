
TESTCASE=t1 t2 t3 t4
TESTDIR=$(PWD)/test
SERVER=$(TESTDIR)/server
CLIENT=$(TESTDIR)/client
BINDIR=$(TESTDIR)/bin

EVALCASE=latency_nor latency_sus latency_mal latency_udp throughput_nor throughput_sus throughput_mal throughput_udp memory attack_udp
EVALDIR=$(PWD)/evaluation
EVALBIN=$(EVALDIR)/bin

CUDPOBJ=cudp.o
TESTOBJ=$(TESTDIR)/testcase.o

all: $(EVALCASE)

$(EVALCASE): $(CUDPOBJ)
	gcc $^ $(EVALDIR)/$@.c -o $(EVALBIN)/$@ -lssl -lcrypto -lpthread

$(TESTCASE): $(CUDPOBJ) $(TESTOBJ)
	gcc $^ $(SERVER)/$@.c -o $(BINDIR)/$@s -lssl -lcrypto -lpthread
	gcc $^ $(CLIENT)/$@.c -o $(BINDIR)/$@c -lssl -lcrypto -lpthread

$(CUDPOBJ): cudp.c
	gcc $^ -c -o $@ -lssl -lcrypto

$(TESTOBJ): $(TESTDIR)/testcase.c
	gcc $^ -c -o $@

clean:
	rm -f *.o $(TESTDIR)/*.o $(BINDIR)/* $(EVALBIN)/* $(EVALDIR)/*.o
