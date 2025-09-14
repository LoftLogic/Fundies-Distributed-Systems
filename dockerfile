FROM gcc:latest

WORKDIR /root/dissys/project1

COPY *.h *.cpp ./
COPY types/ ./types/
COPY hostsfile.txt ./


RUN g++ -std=c++17 -pthread -O2 \
    main.cpp coordinator.cpp tracker.cpp \
    -o coordinator

ENTRYPOINT ["./coordinator"]