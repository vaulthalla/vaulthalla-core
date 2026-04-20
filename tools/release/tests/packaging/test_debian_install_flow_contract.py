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
            "db_get vaulthalla/configure-nginx",
            "has_non_nginx_listener_on_web_ports",
            "has_custom_nginx_sites_enabled",
            "nginx -t >/dev/null 2>&1",
            "safe_systemctl enable --now \"$WEB_SYSTEMD_UNIT\"",
            "install -m 0644 \"$NGINX_TEMPLATE\" \"$NGINX_SITE_AVAIL\"",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, postinst)


if __name__ == "__main__":
    unittest.main()
