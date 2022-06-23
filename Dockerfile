FROM tarscloud/base-compiler

# RUN mkdir -p /data
# COPY . /data
# RUN cd /data \
#     && mkdir -p build   \
#     && cd build    \
#     && cmake .. -DCMAKE_BUILD_TYPE=Release  \
#     && make -j8 

# RUN exec-build-cloud.sh tarscloud/tars.cppbase cpp build/bin/CountServer yaml/values.yaml latest true

# ENV DOCKER_CLI_EXPERIMENTAL=enabled 

# RUN echo "FROM ubuntu:20.04" > Dockerfile
# RUN docker run --rm --privileged tonistiigi/binfmt:latest --install all
# RUN docker buildx create --name builder --driver docker-container  --buildkitd-flags '--allow-insecure-entitlement security.insecure --allow-insecure-entitlement network.host' --use
# RUN docker buildx inspect --bootstrap --builder builder

# RUN docker buildx build . --file Dockerfile --tag test --platform=linux/amd64,linux/arm64