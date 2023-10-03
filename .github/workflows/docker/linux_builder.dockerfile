FROM debian:10-slim

ENV QTROOT=/opt/5.15.2
ENV GITHUB_CI=YES
ENV PACKAGES_KAPPS build-essential ruby-full rake clang-7 git python3 python3-pip arch-test mesa-common-dev libnl-3-dev libnl-route-3-dev libnl-genl-3-dev zlib1g libglib2.0-0 libgl1-mesa-glx patchelf
ENV LANG=en_US.UTF-8

ARG CUSTOM_QT_INSTALLER

RUN apt-get update && apt-get install -y ${PACKAGES_KAPPS} && gem install bundler -v 2.3.17
WORKDIR /opt
RUN if [ ! -z "$CUSTOM_QT_INSTALLER" ]; \
    then \
        wget $CUSTOM_QT_INSTALLER -nv -O qt_installer.sh && chmod +x qt_installer.sh && \
        bash -c "./qt_installer.sh --accept --quiet --noprogress --nox11 --target /tmp/Qt <<< /opt" && \
        rm -f qt_installer.sh; \
    else \
        python3 -m pip install -U pip && python3 -m pip install aqtinstall && python3 -m aqt install-qt linux desktop 5.15.2 gcc_64; \
    fi
RUN git config --global --add safe.directory '*'

WORKDIR /
