-- Database export via SQLPro (https://www.sqlprostudio.com/allapps.html)
-- WARNING: This file may contain descructive statements such as DROPs.
-- Please ensure that you are running the script at the proper location.


-- BEGIN TABLE accounts
DROP TABLE IF EXISTS accounts;
CREATE TABLE `accounts` (
  `iban` varchar(34) NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `balance` float NOT NULL,
  PRIMARY KEY (`iban`),
  UNIQUE KEY `iban` (`iban`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- END TABLE accounts

-- BEGIN TABLE cards
DROP TABLE IF EXISTS cards;
CREATE TABLE `cards` (
  `card_id` varchar(7) NOT NULL,
  `iban` varchar(34) NOT NULL,
  `user_id` int(10) unsigned NOT NULL,
  `pin` int(4) NOT NULL,
  `attempts` tinyint(3) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`card_id`),
  UNIQUE KEY `card_id` (`card_id`),
  KEY `iban` (`iban`),
  KEY `user_id` (`user_id`),
  CONSTRAINT `cards_ibfk_1` FOREIGN KEY (`iban`) REFERENCES `accounts` (`iban`) ON UPDATE CASCADE,
  CONSTRAINT `cards_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`user_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- END TABLE cards

-- BEGIN TABLE registrations
DROP TABLE IF EXISTS registrations;
CREATE TABLE `registrations` (
  `registration_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `user_id` int(10) unsigned NOT NULL,
  `iban` varchar(34) NOT NULL,
  PRIMARY KEY (`registration_id`),
  UNIQUE KEY `registration_id` (`registration_id`),
  KEY `user_id` (`user_id`),
  KEY `iban` (`iban`),
  CONSTRAINT `registrations_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`user_id`) ON UPDATE CASCADE,
  CONSTRAINT `registrations_ibfk_2` FOREIGN KEY (`iban`) REFERENCES `accounts` (`iban`) ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8;

-- END TABLE registrations

-- BEGIN TABLE transactions
DROP TABLE IF EXISTS transactions;
CREATE TABLE `transactions` (
  `transaction_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `status` tinyint(3) unsigned NOT NULL,
  `time` datetime NOT NULL,
  `source_iban` varchar(34) DEFAULT NULL,
  `dest_iban` varchar(34) DEFAULT NULL,
  `amount` bigint(20) NOT NULL,
  PRIMARY KEY (`transaction_id`),
  UNIQUE KEY `transaction_id` (`transaction_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- END TABLE transactions

-- BEGIN TABLE users
DROP TABLE IF EXISTS users;
CREATE TABLE `users` (
  `user_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `first_name` varchar(128) NOT NULL,
  `last_name` varchar(128) NOT NULL,
  PRIMARY KEY (`user_id`),
  UNIQUE KEY `user_id` (`user_id`)
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8;

-- END TABLE users

