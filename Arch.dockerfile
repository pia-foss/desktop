FROM archlinux:latest

ENV PACKAGES_KAPPS archlinux-keyring autoconf automake binutils file findutils gcc gettext grep groff gzip libtool m4 make pacman patch pkgconf sed sudo texinfo which ruby rake clang llvm git python3 python-pip mesa libnl zlib
ENV QTROOT=/5.15.2
ENV PATH=$PATH:/root/.local/share/gem/ruby/3.0.0/bin

RUN pacman -Syu --noconfirm && pacman -Sy --noconfirm ${PACKAGES_KAPPS} && gem install bundler -v 2.3.17
RUN python3 -m pip install -U pip && python3 -m pip install aqtinstall && python3 -m aqt install-qt linux desktop 5.15.2 gcc_64 -m qtwebengine qtnetworkauth

ENV GITHUB_CI=YES
