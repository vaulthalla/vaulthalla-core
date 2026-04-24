import unittest
from unittest import mock

from deploy.lifecycle import main


class ConfigProjectionTests(unittest.TestCase):
    def test_parse_projection_reads_nested_sections(self) -> None:
        text = """
websocket_server:
  enabled: false
  host: 127.0.0.1
  port: 36969
http_preview_server:
  enabled: true
  host: ::1
  port: 36970
database:
  host: db.example.net
  port: 5433
  name: vault
  user: vault_user
  pool_size: 42
"""
        projection = main.parse_config_projection_from_text(text)
        self.assertEqual(projection["websocket_server"]["enabled"], False)
        self.assertEqual(projection["websocket_server"]["host"], "127.0.0.1")
        self.assertEqual(projection["websocket_server"]["port"], 36969)
        self.assertEqual(projection["http_preview_server"]["host"], "::1")
        self.assertEqual(projection["database"]["host"], "db.example.net")
        self.assertEqual(projection["database"]["pool_size"], 42)

    def test_update_database_block_rewrites_existing_keys(self) -> None:
        text = """
database:
  host: localhost
  port: 5432
  name: vaulthalla
  user: vaulthalla

sharing:
  enabled: true
"""
        updated = main.update_database_block_text(
            text,
            {
                "host": "db.example.net",
                "port": 5433,
                "name": "vault",
                "user": "vault_user",
                "pool_size": 15,
            },
        )
        self.assertIn("  host: db.example.net\n", updated)
        self.assertIn("  port: 5433\n", updated)
        self.assertIn("  name: vault\n", updated)
        self.assertIn("  user: vault_user\n", updated)
        self.assertIn("  pool_size: 15\n", updated)
        self.assertIn("sharing:\n", updated)


class DispatchTests(unittest.TestCase):
    def test_setup_db_dispatches(self) -> None:
        parser = main.build_parser()
        args = parser.parse_args(["setup", "db"])
        with mock.patch.object(main, "setup_db", return_value=0) as handler:
            rc = main.dispatch(args)
        self.assertEqual(rc, 0)
        handler.assert_called_once_with(args)

    def test_teardown_nginx_dispatches(self) -> None:
        parser = main.build_parser()
        args = parser.parse_args(["teardown", "nginx"])
        with mock.patch.object(main, "teardown_nginx", return_value=0) as handler:
            rc = main.dispatch(args)
        self.assertEqual(rc, 0)
        handler.assert_called_once_with(args)


if __name__ == "__main__":
    unittest.main()
