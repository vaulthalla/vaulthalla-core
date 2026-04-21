from __future__ import annotations

from pathlib import Path
import unittest


class DebianInstallFlowContractTests(unittest.TestCase):
    def _repo_root(self) -> Path:
        return Path(__file__).resolve().parents[4]

    def test_debian_templates_include_nginx_prompt(self) -> None:
        templates = (self._repo_root() / "debian" / "templates").read_text(encoding="utf-8")
        self.assertIn("Template: vaulthalla/init-db", templates)
        self.assertIn("Template: vaulthalla/superadmin-uid", templates)
        self.assertIn("Template: vaulthalla/configure-nginx", templates)
        self.assertIn("Template: vaulthalla/init-db\nType: boolean\nDefault: false", templates)
        self.assertIn("Template: vaulthalla/configure-nginx\nType: boolean\nDefault: false", templates)

    def test_debian_config_prompts_all_supported_debconf_questions(self) -> None:
        config = (self._repo_root() / "debian" / "config").read_text(encoding="utf-8")
        self.assertIn("db_input high vaulthalla/init-db", config)
        self.assertIn("db_input medium vaulthalla/superadmin-uid", config)
        self.assertIn("db_input medium vaulthalla/configure-nginx", config)
        self.assertIn("db_go", config)

    def test_postinst_contains_web_and_conservative_nginx_flow(self) -> None:
        postinst = (self._repo_root() / "debian" / "postinst").read_text(encoding="utf-8")
        required_fragments = (
            "PENDING_SUPERADMIN_UID_FILE=\"/run/vaulthalla/superadmin_uid\"",
            "WEB_SYSTEMD_UNIT=\"vaulthalla-web.service\"",
            "PSQL_DEPLOY_DIR=\"/usr/share/vaulthalla/psql\"",
            "NGINX_MANAGED_MARKER=\"/var/lib/vaulthalla/nginx_site_managed\"",
            "require_runtime_assets",
            "fail_init_db()",
            "postgresql_server_ready()",
            "db_get vaulthalla/configure-nginx",
            "CONFIGURE_NGINX=\"${RET:-false}\"",
            "INIT_DB=\"${RET:-false}\"",
            "fail_init_db \"Local PostgreSQL server is not accepting connections.\"",
            "has_non_nginx_listener_on_web_ports",
            "has_custom_nginx_sites_enabled",
            "nginx -t >/dev/null 2>&1",
            "printf '%s\\n' \"managed-by=${PKG}\" > \"$NGINX_MANAGED_MARKER\"",
            "ROLE_CREATED=\"false\"",
            "Role already existed; preserving current DB password seed state.",
            "safe_systemctl enable --now \"$CORE_SYSTEMD_UNIT\"",
            "safe_systemctl enable --now \"$WEB_SYSTEMD_UNIT\"",
            "install -m 0644 \"$NGINX_TEMPLATE\" \"$NGINX_SITE_AVAIL\"",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, postinst)

    def test_prerm_handles_remove_and_nginx_symlink_cleanup_conservatively(self) -> None:
        prerm = (self._repo_root() / "debian" / "prerm").read_text(encoding="utf-8")
        required_fragments = (
            "WEB_SYSTEMD_UNIT=\"vaulthalla-web.service\"",
            "NGINX_SITE_AVAIL=\"/etc/nginx/sites-available/vaulthalla\"",
            "NGINX_SITE_ENABLED=\"/etc/nginx/sites-enabled/vaulthalla\"",
            "case \"$1\" in",
            "remove|deconfigure)",
            "safe_systemctl stop \"$WEB_SYSTEMD_UNIT\"",
            "safe_systemctl disable \"$WEB_SYSTEMD_UNIT\"",
            "disable_nginx_site_link_if_package_path",
            "#DEBHELPER#",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, prerm)

    def test_postrm_distinguishes_remove_and_purge_cleanup_boundaries(self) -> None:
        postrm = (self._repo_root() / "debian" / "postrm").read_text(encoding="utf-8")
        required_fragments = (
            "NGINX_MANAGED_MARKER=\"/var/lib/vaulthalla/nginx_site_managed\"",
            "remove)",
            "purge)",
            "cleanup_nginx_site_link_if_package_path",
            "Preserving nginx site file on purge; no package-managed marker present.",
            "rm -rf /var/lib/vaulthalla /var/log/vaulthalla",
            "rmdir /mnt/vaulthalla >/dev/null 2>&1 || true",
            "db_purge || true",
            "#DEBHELPER#",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, postrm)


if __name__ == "__main__":
    unittest.main()
