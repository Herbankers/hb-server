#!/usr/bin/env python3

import mysql.connector
from mysql.connector import Error

print("Python MySQL database CLI")

# globale waardes
isRunning = True
global isDatabase


# print functies
def input_menu():
    keuze = int(input("uw keuze: "))
    return keuze

def errorHandler():
    print("onjuiste invoer, probeer opnieuw!")

def errorHandler_offline_hoofdmenu():
    errorHandler()
    print_hoofdmenu(False)

def errorHandler_online_hoofdmenu():
    errorHandler()
    print_hoofdmenu(True)


def errorHandler_databasemenu():
    errorHandler()
    print_databasemenu()


def print_hoofdmenu(isConnected):
    print("\n-[Hoofdmenu]-")

    isDatabase = isConnected

    if isDatabase:
        print("Connected to database")
        print("Maak een keuze uit:")

        print("1 = Database herconfigureren")
        print("2 = Database views")
        print("3 = Database controls")
        print("9 = script stoppen")

        print(online_hoofdmenu_controller.get(input_menu(), errorHandler_online_hoofdmenu)())

    else:
        print("Maak een keuze uit:")

        print("1 = Database configuraties")
        print("9 = script stoppen")

        print(offline_hoofdmenu_controller.get(input_menu(), errorHandler_offline_hoofdmenu)())


def print_databasemenu():
    print("\n-[Database views]-")
    print("Maak een keuze uit:")

    print("1 = account tonen")
    print("2 = gebruikers tabel tonen")
    print("3 = rekeningen tabel tonen")
    print("4 = passen tabel tonen")

    print("9 = terug naar hoofdmenu")

    print(online_hoofdmenu_controller.get(input_menu(), errorHandler_databasemenu)())


def print_controlsmenu():
    print("\n-[Database controls]-")
    print("Maak een keuze uit:")

    print("1 = gebruiker toevoegen")
    print("2 = gebruiker verwijderen")
    print("3 = nieuwe rekening openen")
    print("4 = rekening blokkeren")
    print("5 = pas toevoegen")
    print("6 = pas deblokkeren")

    print("9 = terug naar hoofdmenu")

    online_hoofdmenu_controller.get(input_menu(), errorHandler_databasemenu())()


def print_afsluiten():
    return "python script gestopt!"


def print_configuratiesetup():
    print("\n-[Database verbinding maken]-")
    dbhostname = input("DB hostname\t: ")
    dbport = input("DB port\t\t: ")
    dbusername = input("DB username\t: ")
    dbpassword = input("DB password\t: ")
    dbname = input("DB database\t: ")
    print("Checking configuration...")

    try:
        mydb = mysql.connector.connect(
            host=dbhostname,
            port=dbport,
            user=dbusername,
            passwd=dbpassword,
            database=dbname
        )
        cursor = mydb.cursor()
        cursor.execute("SELECT VERSION()")
        results = cursor.fetchone()
        # Check if anything at all is returned
        if results:
            print("Connected to database")
            print("Database version: ", results)
            print_hoofdmenu(True)
        else:
            print("Not connected to database")
            print_configuratiesetup()
    except mysql.connector.Error:
        print("Not able to connect to database!!")
        print_configuratiesetup()

# functies switch controllers

online_hoofdmenu_controller = {
    1: print_configuratiesetup,
    2: print_databasemenu,
    3: print_controlsmenu,
    9: print_afsluiten
}

offline_hoofdmenu_controller = {
    1: print_configuratiesetup,
    9: print_afsluiten
}

while isRunning:
    print_hoofdmenu(False)
    break
