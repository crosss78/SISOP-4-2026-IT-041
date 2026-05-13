#!/usr/bin/env bash
set -euo pipefail

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs /run/samba
touch /logs/libraryit.log /logs/samba.log

# Start rsyslog so full_audit can write into /logs
cat >/etc/rsyslog.d/50-libraryit.conf <<'EOF'
local7.*    /logs/libraryit.log
EOF

rsyslogd

# Groups
getent group readonly >/dev/null || groupadd readonly
getent group staff >/dev/null || groupadd staff

ensure_user() {
  local user="$1"
  local group="$2"
  local pass="$3"

  if ! id "$user" >/dev/null 2>&1; then
    useradd -M -s /usr/sbin/nologin -g "$group" "$user"
  fi

  printf '%s\n%s\n' "$pass" "$pass" | smbpasswd -a -s "$user" >/dev/null
  smbpasswd -e "$user" >/dev/null
}

ensure_user member readonly member123
ensure_user contributor staff contrib456
ensure_user librarian staff lib789

# Persistent storage permissions
chown -R root:staff /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs || true
chmod 770 /libraryit/ebooks /libraryit/papers || true
chmod 750 /libraryit/sourcecode || true
chmod 555 /libraryit/docs || true

# Samba runtime dirs
mkdir -p /var/run/samba /var/cache/samba /var/lib/samba

# Start SMB services
/usr/sbin/nmbd -F --no-process-group &
exec /usr/sbin/smbd -F --no-process-group