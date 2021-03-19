--
-- WARNING!
-- Make sure the default character set is UTF-8
-- See https://mariadb.com/kb/en/library/setting-character-sets-and-collations/#example-changing-the-default-character-set-to-utf-8 for more info
--

CREATE DATABASE IF NOT EXISTS `herbankdb`;
USE herbankdb;

CREATE TABLE IF NOT EXISTS `users` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`first_name`	VARCHAR(128)		NOT NULL,
	`last_name`	VARCHAR(128)		NOT NULL,
	PRIMARY KEY (`id`)
);

CREATE TABLE IF NOT EXISTS `accounts` (
	`iban`		VARCHAR(34)		NOT NULL UNIQUE,
	`user_id`	INTEGER UNSIGNED	NOT NULL,
	`type`		TINYINT UNSIGNED	NOT NULL,
	`balance`	BIGINT			NOT NULL,
	PRIMARY KEY (`iban`),
	FOREIGN KEY (`user_id`) REFERENCES `users` (`id`)
		ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS `registrations` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`user_id`	INTEGER UNSIGNED	NOT NULL,
	`iban`		VARCHAR(34)		NOT NULL,
	PRIMARY KEY (`id`),
	FOREIGN KEY (`user_id`) REFERENCES `users` (`id`)
		ON DELETE RESTRICT ON UPDATE CASCADE,
	FOREIGN KEY (`iban`) REFERENCES `accounts` (`iban`)
		ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS `cards` (
	`id`		BINARY(12)		NOT NULL UNIQUE,
	`user_id`	INTEGER UNSIGNED	NOT NULL,
	`card_number`	INTEGER UNSIGNED	NOT NULL,
	`pin`		BINARY(128)		NOT NULL,
	`attempts`	TINYINT UNSIGNED	NOT NULL,
	PRIMARY KEY (`id`),
	FOREIGN KEY (`user_id`) REFERENCES `users` (`id`)
		ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS `transactions` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`status`	TINYINT UNSIGNED	NOT NULL,
	`time`		DATETIME		NOT NULL,
	`source_iban`	VARCHAR(34),
	`dest_iban`	VARCHAR(34),
	`amount`	BIGINT			NOT NULL,
	PRIMARY KEY (`id`)
);

--
-- WARNING!
-- Change the passwords before loading this file
--

-- This user is used by hb-server
CREATE USER 'hb-server'@'localhost' IDENTIFIED BY 'password';
GRANT SELECT ON `herbankdb`.* TO `hb-server`@`localhost`;
GRANT INSERT ON `herbankdb`.`transactions` TO `hb-server`@`localhost`;
GRANT UPDATE ON `herbankdb`.`cards` TO `hb-server`@`localhost`;
GRANT UPDATE ON `herbankdb`.`accounts` TO `hb-server`@`localhost`;

-- This user is used by hb-cli
CREATE USER 'hb-cli'@'localhost' IDENTIFIED BY 'password';
GRANT DELETE,INSERT,SELECT,UPDATE ON `herbankdb`.* TO `hb-cli`@`localhost`;
