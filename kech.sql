--
-- WARNING!
-- Make sure the default character set is UTF-8
-- See https://mariadb.com/kb/en/library/setting-character-sets-and-collations/#example-changing-the-default-character-set-to-utf-8 for more info
--

CREATE DATABASE IF NOT EXISTS `kech`;
USE kech;

CREATE TABLE IF NOT EXISTS `customers` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`first_name`	VARCHAR(128)		NOT NULL,
	`last_name`	VARCHAR(128)		NOT NULL,
	`last_card_id`	INTEGER UNSIGNED	NOT NULL AUTO_INCREMENT,
	PRIMARY KEY (`id`)
);

CREATE TABLE IF NOT EXISTS `cards` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`customer_id`	INTEGER UNSIGNED	NOT NULL,
	`card_id`	INTEGER UNSIGNED	NOT NULL,
	`pin`		INTEGER(6) UNSIGNED	NOT NULL, -- FIXME XXX XXX XXX XXX XXX XXX XXX XXX No
	`blocked`	BOOLEAN			NOT NULL,
	PRIMARY KEY (`id`),
	FOREIGN KEY (`customer_id`) REFERENCES `customers` (`id`)
		ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS `accounts` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`customer_id`	INTEGER UNSIGNED	NOT NULL,
	`type`		TINYINT UNSIGNED	NOT NULL,
	`iban`		VARCHAR(34)		NOT NULL UNIQUE,
	`balance`	DECIMAL(15,2)		NOT NULL,
	PRIMARY KEY (`id`),
	FOREIGN KEY (`customer_id`) REFERENCES `customers` (`id`)
		ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS `transactions` (
	`id`		INTEGER UNSIGNED	NOT NULL UNIQUE AUTO_INCREMENT,
	`status`	TINYINT UNSIGNED	NOT NULL,
	`src_iban`	VARCHAR(34),
	`dst_iban`	VARCHAR(34),
	`amount`	DECIMAL(15,2)		NOT NULL,
	PRIMARY KEY (`id`)
);
