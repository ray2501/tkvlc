%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tkvlc
Summary:       A demo to embed libVLC to Tk toolkit widget
Version:       0.3
Release:       0
License:       MIT
Group:         Development/Libraries/Tcl
Source:        %{name}-%{version}.tar.gz
URL:           https://github.com/ray2501/tkvlc
BuildRequires: autoconf
BuildRequires: make
BuildRequires: tcl-devel >= 8.5
BuildRequires: tk-devel >= 8.5
BuildRequires: vlc-devel
BuildRoot:     %{buildroot}

%description
A demo to embed libVLC to Tk toolkit widget.

%prep
%setup -q -n %{name}-%{version}

%build
./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{tcl_archdir}/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{tcl_archdir}
