FROM gcc:13

WORKDIR /app

RUN apt-get update && apt-get install -y wget

RUN wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -O json.hpp
RUN wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O httplib.h

COPY main.cpp .

RUN g++ -O3 -pthread -std=c++17 main.cpp -o battlesnake

EXPOSE 8000

CMD ["./battlesnake"]