# RPM Spec file for tarmux

Name:      tarmux
Version:   1.0.0
Release:   1%{?dist}
Summary:   Multiplex / demultiplex streams
License:   ASL v2.0
Group:     Applications/System
Source:    https://github.com/minfrin/%{name}/releases/download/%{name}-%{version}/%{name}-%{version}.tar.bz2
URL:	   https://github.com/minfrin/tarmux
BuildRequires: libarchive-devel >= 3, help2man

%description
Multiplex and demultiplex streams using tar file fragments.

%prep
%setup -q
%build
%configure
%make_build

%install
%make_install

%files
%{_bindir}/tardemux
%{_bindir}/tarmux
%{_mandir}/man1/tardemux.1*
%{_mandir}/man1/tarmux.1*

%doc AUTHORS ChangeLog NEWS README
%license COPYING

%changelog
* Sun May 22 2016 Graham Leggett <minfrin@sharp.fm> - 1.0.0-1
- Initial version of the package
