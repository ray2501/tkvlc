%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tkvlc
Summary:       A demo to embed libVLC to Tk toolkit widget
Version:       0.3
Release:       2
License:       MIT
Group:         Development/Libraries/Tcl
Source:        https://github.com/ray2501/tkvlc/tkvlc_0.3.zip
URL:           https://github.com/ray2501/tkvlc
Buildrequires: tcl-devel >= 8.5
Buildrequires: tk-devel >= 8.5
Buildrequires: vlc-devel
BuildRoot:     %{buildroot}

%description
A demo to embed libVLC to Tk toolkit widget.

%prep
%setup -q -n %{name}

%build
./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{directory}/%{_lib}/tcl/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{directory}/%{_lib}/tcl
