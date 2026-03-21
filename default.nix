{ stdenv
, lib
, fetchFromGitHub
, cmake
, pkg-config
, networkmanager
, libnl
, openssl
, git
, SDL2
, SDL2_ttf
, SDL2_image
, libwebp
, libtiff
, ffmpeg
, polkit
, libxml2
, xorg
, libGL
, libdrm
}:
let
  hostap = fetchFromGitHub {
    owner = "rolandoislas";
    repo = "drc-hostap";
    rev = "418e5e206786de2482864a0ec3a59742a33b6623";
    hash = "sha256-kAv/PetD6Ia5NzmYMWWyWQll1P+N2bL/zaV9ATiGVV0=";
    leaveDotGit = true;
  };
in
stdenv.mkDerivation {
  pname = "vanilla-wiiu";
  version = "continuous";

  src = ./.;

  nativeBuildInputs = [
    cmake
    pkg-config
    git
  ];
  buildInputs = [
    networkmanager
    libnl
    openssl
    SDL2
    SDL2_ttf
    SDL2_image
    libwebp
    libtiff
    ffmpeg
    polkit
    libxml2
    xorg.libX11
    libGL
    libdrm
  ];

  postPatch = ''
    substituteInPlace cmake/FindLibNL.cmake \
      --replace-fail /usr/include/libnl3 ${lib.getDev libnl}/include/libnl3
    substituteInPlace pipe/linux/CMakeLists.txt \
      --replace-fail "https://github.com/rolandoislas/drc-hostap.git" "${hostap}" \
      --replace-fail "--branch master" "--branch fetchgit"
  '';

  meta = {
    description = "A software clone of the Wii U gamepad for Linux";
    homepage = "https://github.com/vanilla-wiiu/vanilla";
    license = lib.licenses.gpl2;
    mainProgram = "vanilla";
    platforms = lib.platforms.linux;
  };
}
