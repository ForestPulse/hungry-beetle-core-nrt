FROM ghcr.io/osgeo/gdal:ubuntu-small-3.10.2 AS builder

# disable interactive frontends
ENV DEBIAN_FRONTEND=noninteractive 

# Environment variables
ENV INSTALL_DIR=/install_dir
ENV SOURCE_DIR=/source_dir

# Copy src to SOURCE_DIR
RUN mkdir -p $SOURCE_DIR $INSTALL_DIR
WORKDIR $SOURCE_DIR
COPY . .

RUN apt-get -y update && apt-get -y upgrade && \
apt-get -y install \
  build-essential \
  r-base && \
Rscript -e "install.packages('pak', repos='https://r-lib.github.io/p/pak/dev/')" && \
Rscript -e "pak::pkg_install(c('rmarkdown','plotly', 'dplyr', 'terra'))" && \
apt-get clean && rm -r /var/cache/

# Create a dedicated 'docker' group and user
RUN groupadd docker && \
useradd -m docker -g docker -p docker && \
chmod 0777 /home/docker && \
chgrp docker /usr/local/bin && \
mkdir -p /home/docker/bin && chown docker /home/docker/bin

# Build, install
RUN echo "building hungry-beetle" && \
  make && \
  make DINSTALL=$INSTALL_DIR install 

FROM ghcr.io/forestpulse/hungry-beetle-core-nrt:latest AS final

COPY --chown=docker:docker --from=builder $INSTALL_DIR $HOME/bin

RUN rm -rf $SOURCE_DIR $INSTALL_DIR

# Use this user by default
USER docker
ENV HOME=/home/docker
ENV PATH="$PATH:/home/docker/bin"
WORKDIR /home/docker
CMD ["echo 'beetle hungry, nom nom'"]
