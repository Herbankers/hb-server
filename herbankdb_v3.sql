CREATE DATABASE IF NOT EXISTS herbankdb;
USE herbankdb;

create table if not exists accounts
(
    iban    varchar(34)      not null,
    user_id int unsigned     null,
    type    tinyint unsigned not null,
    balance bigint           not null,
    constraint iban
        unique (iban)
);

alter table accounts
    add primary key (iban);

create table if not exists transactions
(
    transaction_id int unsigned auto_increment,
    status         tinyint unsigned not null,
    time           datetime         not null,
    source_iban    varchar(34)      null,
    dest_iban      varchar(34)      null,
    amount         bigint           not null,
    constraint transaction_id
        unique (transaction_id)
);

alter table transactions
    add primary key (transaction_id);

create table if not exists users
(
    id         int unsigned auto_increment,
    first_name varchar(128) not null,
    last_name  varchar(128) not null,
    constraint user_id
        unique (id)
);

alter table users
    add primary key (id);

create table if not exists cards
(
    id       binary(12)                 not null,
    iban     varchar(34)                not null,
    user_id  int unsigned               not null,
    pin      binary(128)                not null,
    attempts tinyint unsigned default 0 not null,
    constraint card_id
        unique (id),
    constraint cards_ibfk_1
        foreign key (iban) references accounts (iban)
            on update cascade,
    constraint cards_ibfk_2
        foreign key (user_id) references users (id)
            on update cascade
);

create index iban
    on cards (iban);

create index user_id
    on cards (user_id);

alter table cards
    add primary key (id);

create table if not exists registrations
(
    registration_id int unsigned auto_increment,
    user_id         int unsigned not null,
    iban            varchar(34)  not null,
    constraint registration_id
        unique (registration_id),
    constraint registrations_ibfk_1
        foreign key (user_id) references users (id)
            on update cascade on delete cascade,
    constraint registrations_ibfk_2
        foreign key (iban) references accounts (iban)
            on update cascade on delete cascade
);

alter table registrations
    add primary key (registration_id);

--
-- WARNING!
-- Change the passwords before loading this file
--

-- This user is used by hb-server
CREATE USER 'hb-server'@'localhost' IDENTIFIED BY 'password';
GRANT SELECT ON herbankdb.* TO 'hb-server'@'localhost';
GRANT INSERT ON herbankdb.transactions TO 'hb-server'@'localhost';
GRANT UPDATE ON herbankdb.cards TO 'hb-server'@'localhost';
GRANT UPDATE ON herbankdb.accounts TO 'hb-server'@'localhost';

-- This user is used by hb-cli
CREATE USER 'hb-cli'@'localhost' IDENTIFIED BY 'password';
GRANT DELETE,INSERT,SELECT,UPDATE ON herbankdb.* TO 'hb-cli'@'localhost';