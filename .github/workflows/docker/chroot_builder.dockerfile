FROM debian:10-slim

ENV QTROOT=/opt/5.15.2
ENV GITHUB_CI=YES
ENV PACKAGES_KAPPS debootstrap schroot curl git git-lfs python3 python3-pip
ENV LANG=en_US.UTF-8

ARG CROSS_ARCH

RUN apt-get update && apt-get install -y ${PACKAGES_KAPPS} && git lfs install --system
WORKDIR /opt

# We need both the x86_64 and arch-specific builds of Qt
RUN ( \
        PIA_QT_INSTALLER_URL="https://privateinternetaccess-storage.s3.amazonaws.com/pub/pia_desktop/qt/2021-01-23/qt-5.15.2-pia-linux-${CROSS_ARCH}.run" && \
        wget $PIA_QT_INSTALLER_URL -nv -O qt_installer_${CROSS_ARCH}.sh && chmod +x qt_installer_${CROSS_ARCH}.sh && \
        bash -c "./qt_installer_${CROSS_ARCH}.sh --accept --quiet --noprogress --nox11 --target /tmp/Qt <<< /opt" && \
        rm -f qt_installer_${CROSS_ARCH}.sh \
    ) & \
    ( \
        PIA_QT_INSTALLER_URL="https://privateinternetaccess-storage.s3.amazonaws.com/pub/pia_desktop/qt/2021-01-23/qt-5.15.2-pia-linux-x86_64.run" && \
        wget $PIA_QT_INSTALLER_URL -nv -O qt_installer_x86_64.sh && chmod +x qt_installer_x86_64.sh && \
        bash -c "./qt_installer_x86_64.sh --accept --quiet --noprogress --nox11 --target /tmp/Qt_x86_64 <<< /opt" && \
        rm -f qt_installer_x86_64.sh \
    ) & \
    echo Waiting for Qt Installations; \
    wait; 
RUN addgroup crontab
RUN git config --global --add safe.directory '*'

WORKDIR /
