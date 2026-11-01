FROM gcc:14-bookworm
WORKDIR /app
COPY . .
RUN g++ -std=c++11 -O2 dictionary.cpp -o dictionary
EXPOSE 10000
CMD ["./dictionary"]
