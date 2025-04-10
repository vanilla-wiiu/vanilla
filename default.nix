{ stdenv
, lib
, fetchFromGitHub
, cmake
, pkg-config
, git
, openssl
, gtk4
, networkmanager
, libsysprof-capture
, pcre2
, util-linux
, libselinux
, libsepol
, libnl
, SDL2
, SDL2_ttf
, SDL2_image
, ffmpeg
, libxml2
, cacert
}:
let
  hostap = fetchFromGitHub {
    owner = "rolandoislas";
    repo = "drc-hostap";
    rev = "418e5e206786de2482864a0ec3a59742a33b6623";
    hash = "sha256-0LZLNhGF5OC0AkVRFyP+vtjPJ5VEeIJF9ZfBpoJZdH4=";
  };
in
stdenv.mkDerivation {
  version = "0.0.0";
  name = "vanilla";
  src = ./.;

  nativeBuildInputs = [
    cmake
    pkg-config
    git

    cacert
  ];
  buildInputs = [
    openssl
    gtk4
    networkmanager
    libsysprof-capture
    pcre2
    util-linux
    libselinux
    libsepol
    libnl
    SDL2
    SDL2_ttf
    SDL2_image
    ffmpeg
    libxml2
  ];

  configurePhase = ''
    mkdir build
    cd build
    cmake ..
  '';

  buildPhase = ''
    cp -r ${hostap} ./hostap
    cmake --build .
  '';

  # upstream removed pkg-config support and uses dlopen now
  postPatch =
    let
      libnlPath = lib.getLib libnl;
    in
    lib.optionalString stdenv.hostPlatform.isLinux ''
      substituteInPlace cmake/FindLibNL.cmake \
      --replace-fail /usr/include/libnl3 ${lib.getDev libnl}/include/libnl3
          #substituteInPlace rpi/CMakeLists.txt \
          #--replace-fail "SDL2::SDL2main" ""
    '';

  installPhase = ''
    mkdir -p $out
    cp -r ./lib $out
    cp -r ./bin $out
  '';

  meta = with lib; {
    description = "A software clone of the Wii U gamepad for Linux";
    homepage = "https://github.com/vanilla-wiiu/vanilla";
    license = licenses.gpl2;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
    mainProgram = "vanilla-gui";
  };
}
