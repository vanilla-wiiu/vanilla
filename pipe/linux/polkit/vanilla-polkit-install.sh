#!/bin/bash

set -e

if [ "$EUID" -ne 0 ]
then
  echo "$0 must be run as root"
  exit 1
fi
POLKIT_ACTION_DST="/usr/share/polkit-1/actions/com.mattkc.vanilla.policy"
POLKIT_RULE_DST="/usr/share/polkit-1/rules.d/com.mattkc.vanilla.rules"

if [ "$1" == "--uninstall" ]; then
	sudo rm -vf $PLKIT_ACTION_DST POLKIT_RULE_DST
fi

echo "--------------------------------------------------------------------------------"
echo "vanilla-pipe Polkit Install Script"
echo "--------------------------------------------------------------------------------"
echo "This script will install a Polkit action and rule that will allow vanilla-pipe"
echo "to run as root without entering a password."
echo ""
echo "This makes usage of Vanilla much more convenient, especially on touch-based"
echo "devices such as the Nintendo Switch and Steam Deck, however THERE ARE INHERENT"
echo "RISKS TO ALLOWING A PROGRAM TO RUN AS ROOT WITHOUT USER INTERVENTION. WE MAKE"
echo "NO SAFETY GUARANTEES IF YOU CHOOSE TO CONTINUE WITH THIS PROCESS."
echo ""
read -p "If you understand the risks and wish to continue, type \"I agree\": " agree

if [ "${agree,,}" != "i agree" ]
then
  echo "Exiting"
  exit 1
fi

echo "Continuing..."

while [ 1 ]
do
  read -p "Enter the absolute path of vanilla-pipe (e.g. /usr/bin/vanilla-pipe): " path

  if [ -z "${path}" ]
  then
    read -p "Do you wish to exit? [yn] " exit_confirm
    if [ "${exit_confirm,,}" = "y" ]
    then
      exit 1
    else
      continue
    fi
  fi

  if [ ! -f "${path}" ]
  then
    echo "File \"${path}\" does not exist"
    continue
  fi

  if [ ${path:0:1} != "/" ]
  then
    echo "Path \"${path}\" does not appear to be an absolute path"
    continue
  fi

  read -p "vanilla-pipe will be located at \"$path\", is that correct? [yn] " confirm
  if [ "${confirm,,}" = "y" ]
  then
    break
  fi
done

echo ""

POLICY_TEMPLATE='<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN" "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <vendor>MattKC</vendor>
  <vendor_url>https://mattkc.com</vendor_url>
  <action id="com.mattkc.vanilla">
    <description>Run Vanilla Pipe as root</description>
    <message>Authentication is required to run Vanilla Pipe as root</message>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>auth_admin</allow_inactive>
      <allow_active>auth_admin</allow_active>
    </defaults>
    <annotate key="org.freedesktop.policykit.exec.path">@PIPE_BIN@</annotate>
    <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
  </action>
</policyconfig>'

RULES_TEMPLATE='/**
 * Allow all users to run vanilla-pipe as root without having to enter a password
 *
 * This provides convenience, especially on platforms more suited for touch
 * controls, however it could be dangerous to allow a program unrestricted
 * administrator access, so it should be used with caution.
 */
polkit.addRule(function(action, subject) {
    if (action.id == "com.mattkc.vanilla") {
        return polkit.Result.YES;
    }
});'

echo "Installing Polkit action to ${POLKIT_ACTION_DST}"
echo "$POLICY_TEMPLATE" | sed "s|@PIPE_BIN@|${path}|g" > "${POLKIT_ACTION_DST}"

echo "Installing Polkit rule to ${POLKIT_RULE_DST}"
echo "$RULES_TEMPLATE" > "${POLKIT_RULE_DST}"

echo ""
echo "Installation done. If you wish to undo this, simply run this script
with the --uninstall flag."
