# Use Ubuntu
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y build-essential cmake g++ 

# Set working directory
WORKDIR /mmw/
COPY ./ /mmw/
RUN mkdir -p build/

# App name
ARG APP_NAME
RUN cd build/ && cmake ../ && make && chmod +x broker publish subscribe
