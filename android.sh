#!/bin/bash
set -e
TOP="$(pwd)"
prefix="${HOME}/opt/android/21"
VERSION="1.1.0"
SDLVERSION="2.30.3"
PACKAGE="${2}"
if [[ "x${PACKAGE}" = "x" ]]
then
  echo "usage: $0 prepare app.package.name"
  echo "usage: $0 debug   app.package.name"
  echo "usage: $0 release app.package.name"
  exit 1
fi
if [[ "$1" =~ "prepare" ]]
then
  mkdir -p "${TOP}/android"
  cd "${TOP}/android"
  mkdir -p arm64-v8a armeabi-v7a x86 x86_64
  for d in lib include
  do
    ln -fs "${prefix}/aarch64/$d/" "${TOP}/android/arm64-v8a/$d"
    ln -fs "${prefix}/armv7a/$d/" "${TOP}/android/armeabi-v7a/$d"
    ln -fs "${prefix}/i686/$d/" "${TOP}/android/x86/$d"
    ln -fs "${prefix}/x86_64/$d/" "${TOP}/android/x86_64/$d"
  done
  ln -fs ../../imgui/
  ln -fs ../../imgui-filebrowser/
  ln -fs ../ dirt
  wget -c "https://github.com/libsdl-org/SDL/releases/download/release-${SDLVERSION}/SDL2-${SDLVERSION}.tar.gz"
  tar xaf "SDL2-${SDLVERSION}.tar.gz"
  cd "SDL2-${SDLVERSION}/build-scripts"
  ./androidbuild.sh "${PACKAGE}" ../../dirt/android.c
  cd "../build/${PACKAGE}/"
  rm -rf app
  cp -avf "${TOP}/app.in" app
  sed -i -e "s|VERSION|${VERSION}|g" -e "s|PACKAGE|${PACKAGE}|g" "app/build.gradle"

  mkdir -p "app/src/main/res/mipmap-mdpi"
  rsvg-convert "${TOP}/logo.svg" --width=48 --height=48 > "app/src/main/res/mipmap-mdpi/ic_launcher.png"
  mkdir -p "app/src/main/res/mipmap-hdpi"
  rsvg-convert "${TOP}/logo.svg" --width=72 --height=72 > "app/src/main/res/mipmap-hdpi/ic_launcher.png"
  mkdir -p "app/src/main/res/mipmap-xhdpi"
  rsvg-convert "${TOP}/logo.svg" --width=96 --height=96 > "app/src/main/res/mipmap-xhdpi/ic_launcher.png"
  mkdir -p "app/src/main/res/mipmap-xxhdpi"
  rsvg-convert "${TOP}/logo.svg" --width=144 --height=144 > "app/src/main/res/mipmap-xxhdpi/ic_launcher.png"
  mkdir -p "app/src/main/res/mipmap-xxxhdpi"
  rsvg-convert "${TOP}/logo.svg" --width=192 --height=192 > "app/src/main/res/mipmap-xxxhdpi/ic_launcher.png"

  mkdir -p "app/src/main/java/$(echo "${PACKAGE}" | sed 's|\.|/|g')/"
  cat <<EOF > "app/src/main/java/$(echo "${PACKAGE}" | sed 's|\.|/|g')/DirtActivity.java"
package $2;

import org.libsdl.app.SDLActivity;

public class DirtActivity extends SDLActivity
{
}
EOF
  mkdir -p "app/src/main/res/values/"
  cat <<EOF > app/src/main/res/values/strings.xml
<resources>
    <string name="app_name">Dirt</string>
</resources>
EOF
  cat <<EOF > app/src/main/res/values/styles.xml
<resources>
    <style name="AppTheme" parent="android:Theme.Holo.Light.DarkActionBar">
    </style>
</resources>
EOF
  cat <<EOF > app/src/main/res/values/colors.xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="colorPrimary">#3F51B5</color>
    <color name="colorPrimaryDark">#303F9F</color>
    <color name="colorAccent">#FF4081</color>
</resources>
EOF
  mkdir -p app/jni/SDL
  ln -fs "${TOP}/android/dirt/" app/jni/src
  ln -fs "${TOP}/android/SDL2-${SDLVERSION}/include" app/jni/SDL/include
  ln -fs "${TOP}/android/SDL2-${SDLVERSION}/src" app/jni/SDL/src
  ln -fs "${TOP}/android/SDL2-${SDLVERSION}/android-project/app/src/main/java/org" app/src/main/java/org
else
  cd "${TOP}/android/src/SDL2-${SDLVERSION}/build/${PACKAGE}"
  if [[ "$1" =~ "release" ]]
  then
    if [ ! -e "~/.${PACKAGE}.ks" ]
    then
      echo "you need a signing keystore in ~/.${PACKAGE}.ks"
      echo "you can create one with: keytool -genkeypair -v -keystore ~/.${PACKAGE}.ks -alias dirt -keyalg RSA -keysize 2048 -validity 10000"
      exit 1
    fi
    ./gradlew assembleRelease
    cd app/build/outputs/apk/release/
    zipalign -v -p 4 app-release-unsigned.apk app-release-unsigned-aligned.apk
    apksigner sign --ks "~/.${PACKAGE}.ks" --out "$2-${VERSION}.apk" app-release-unsigned-aligned.apk
    cp -avi "$2-${VERSION}.apk" "${TOP}"
    adb install -r "$2-${VERSION}.apk"
  else
    if [[ "$1" =~ "debug" ]]
    then
      ./gradlew installDebug
    fi
  fi
fi
