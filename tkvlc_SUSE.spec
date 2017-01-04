%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tkvlc
Summary:       A demo to embed libVLC to Tk toolkit widget
Version:       0.1
Release:       2
License:       MIT
Group:         Development/Libraries/Tcl
Source:        https://github.com/ray2501/tkvlc/tkvlc_0.1.zip
URL:           https://github.com/ray2501/tkvlc
Buildrequires: tcl >= 8.5
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
