sudo -u postgres -i psql

create user restreamer;
create user restreamer_admin;
create database restreamer with owner=restreamer_admin;

\q


sudo -u postgres -i psql restreamer

create extension if not exists "uuid-ossp";

revoke all on schema PUBLIC from PUBLIC;
grant usage on schema PUBLIC to PUBLIC;
grant all on schema PUBLIC to restreamer_admin;

\q


sudo -u restreamer_admin -i psql restreamer

create table SERVER
(
    ID serial not null primary key,
    HOST varchar(80) not null,
    CONTROL_PORT smallint not null,
    STATIC_PORT smallint not null,
    RESTREAM_PORT smallint not null,
    CERTIFICATE text not null
);

create table USERS
(
    ID serial not null primary key,
    LOGIN varchar(50) not null,
    SALT varchar(10) not null,
    HASH_TYPE numeric(1) not null,
    PASSWORD_HASH varchar(20) not null
);

create table DEVICES
(
    ID uuid not null primary key default uuid_generate_v1mc(),
    CERTIFICATE text not null,
    DROPBOX_TOKEN varchar(64) default null

--    OWNER integer default null references USERS(ID)
);

create table SOURCES
(
    ID uuid not null primary key default uuid_generate_v1mc(),
    URI varchar(200) not null,
    DROPBOX_STORAGE integer default null,

    DEVICE_ID uuid not null references DEVICES(ID)
);

create table RIGHTS
(
    USER_ID integer not null references USERS(ID),
    SOURCE_ID uuid not null references SOURCES(ID),
    primary key(USER_ID, SOURCE_ID)
);

grant select on all tables in schema PUBLIC to PUBLIC;
grant usage, select on all sequences in schema PUBLIC to PUBLIC;

\q
