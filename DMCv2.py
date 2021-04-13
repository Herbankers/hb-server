#!/usr/bin/env python3

# pip install mysql-connector
import mysql.connector
from mysql.connector import Error

# pip install getpass
import getpass

# pip install passlib
# from passlib.hash import argon2

# pip install argon2-cffi
from argon2 import PasswordHasher

print("Python with MySQL database CLI toolset")

# (voorgedefiniëerde) globale waardes
dbhostname = "localhost"
dbport = 3306
dbusername = "hb-cli"
dbpassword = "password"
dbname = "herbankdb"

ph = PasswordHasher()
isRunning = True
global isDatabase  # standaard true
global db_user_id

# zolang de scipts draait
while isRunning:
    print("\n-[Database verbinding maken]-")
    # user-input voor database gegevens
    # dbhostname  = input("DB hostname\t: ")
    # dbport      = input("DB port\t\t: ")
    # dbusername  = input("DB username\t: ")
    # dbpassword  = input("DB password\t: ")
    # dbname      = input("DB database\t: ")

    print("Checking configuration...")
    # probeer om verbinding te maken met database
    try:
        mydb = mysql.connector.connect(
            host=dbhostname,
            port=dbport,
            user=dbusername,
            passwd=dbpassword,
            database=dbname
        )
        # open de verbinding naar database
        cursor = mydb.cursor()
        # stuur een query op naar de database
        cursor.execute("SELECT VERSION()")
        # haalt antwoord op van database
        results = cursor.fetchone()
        # check of er iets terugkomt
        if results:
            print("Connected to database: ", dbname)
            print("Database version: ", results)
            isDatabase = True
            # Zolang de database is verbonden
            while isDatabase:

                print("\n-[Database keuzemenu]-")
                print("Maak een keuze uit:")
                print("0 = account bekijken")
                print("1 = gebruiker toevoegen")
                print("2 = gebruiker verwijderen")
                print("3 = nieuwe rekening openen")
                print("4 = rekening saldo wijzigen")
                print("5 = pas toevoegen")
                print("6 = pas blokkeren")
                print("7 = pas deblokkeren")
                print("9 = script stoppen")

                # check of input van gebruiker een INTERGER is anders opnieuw vragen naar input
                try:
                    keuze = int(input("Uw keuze: "))
                except ValueError:
                    print("[!] Onjuiste invoer, probeer opnieuw")
                    # bij onjuiste invoer vraag opnieuw naar input van gebruiker
                    continue

                # check of de input van de gebruiker 0 is, ga dan verder met onderstaande
                if keuze == 0:
                    print("\n-[Gebruiker bekijken]-")

                    # check of input van gebruiker een INTERGER is anders opnieuw vragen naar input
                    try:
                        db_gebruikerid = int(input("GebruikerID\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        # bij onjuiste invoer vraag opnieuw naar input van gebruiker
                        continue

                    # check of de verbinding naar de database open is om query uit te voeren anders geef in foutmelding aan wat er mis gaat
                    try:
                        try:
                            # selecteer de gebruikersid, gebruikersnaam, wachtwoord, rekeningnummer, saldo, pasnummer, pincode en pogingen van de tabel gebruikers, rekeningen en pasjes waar de gebruikerid overeenkomt met de input van de gebruiker
                            cursor.execute(
                                """SELECT users.id, first_name, last_name, accounts.iban, type, balance, cards.id, pin, attempts FROM users JOIN registrations ON users.id = registrations.user_id JOIN accounts ON accounts.iban = registrations.iban JOIN cards ON cards.user_id = users.id WHERE users.id = '%s'""" % (
                                    db_gebruikerid))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            # haal resultaat op van database
                            myresult = cursor.fetchall()
                            # print resultaat uit
                            for x in myresult:
                                print("ID\t\t\t\t:", x[0])
                                print("Voornaam\t\t:", x[1])
                                print("Achternaam\t\t:", x[2])
                                print("IBAN\t\t\t:", x[3])
                                print("Type\t\t\t:", x[4])
                                print("Saldo\t\t\t:", x[5])
                                print("Pasnummer\t\t:", x[6])
                                print("Pincode\t\t\t:", x[7])
                                print("Pogingen\t\t:", x[8])
                            # maak verstuurde query naar database permanent
                            mydb.commit()
                            # print success/gefaald als de database reageer
                            if cursor.rowcount > 0:
                                print("[i] Gebruiker ", db_gebruikerid, " succesvol opgehaald!")
                            else:
                                print("[!] Gebruikergegevens van ", db_gebruikerid, " is niet gevonden!")
                        # bij typfouten van de database print het een foutmelding van spellingsfout
                        except TypeError as e:
                            print(e)

                    finally:
                        # als bovenstaande is uitgevoerd ga terug naar begin binnen huidige while loop
                        continue

                # check of de input van de gebruiker 1 was
                if keuze == 1:
                    print("\n-[Gebruiker toevoegen]-")

                    # input voor tabel users
                    try:
                        db_first_name = str(input("Voornaam\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue
                    try:
                        db_last_name = str(input("Achternaam\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                        # input voor tabel accounts
                    try:
                        db_iban = str(input("IBAN\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue
                    try:
                        db_type = int(input("Type\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue
                    try:
                        db_balance = float(input("Saldo\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                        # input voor tabel card
                    try:
                        db_card_id = str(input("Pasnummer\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue
                    try:
                        db_pin = int(input("Pin\t\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        # Query voor het invullen van users
                        try:
                            print("[i] Nieuwe gebruiker aan het toevoegen!")
                            cursor.execute("INSERT INTO users (first_name,last_name) VALUES (%s,%s)",
                                           (db_first_name, db_last_name))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)

                        # Resultaat van Query voor het versturen van first_name en last_name
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Nieuwe gebruiker toevoegen, gelukt!")
                            else:
                                print("[i] Nieuwe gebruiker toevoegen, mislukt!")
                        except TypeError as e:
                            print(e)

                        # Query voor het ophalen van user_id
                        try:
                            print("[i] gebruiker id aan het ophalen!")
                            cursor.execute("SELECT id FROM users WHERE first_name = %s AND last_name = %s",
                                           (db_first_name, db_last_name))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)

                        # Resultaat van Query voor het ophalen van user_id
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                db_user_id = results[0]
                                print("[i] gebruiker id gevonden!")
                                print("user_id: ", results[0], " van ", db_first_name, db_last_name, )
                            else:
                                print("user_id van ", db_first_name, " ", db_last_name, " niet gevonden")
                        except TypeError as e:
                            print(e)

                        # Query voor maken van een rekening
                        try:
                            print("[i] Nieuwe account voor gebruiker aan het maken!")
                            cursor.execute("INSERT INTO accounts (iban,user_id,type,balance) VALUES (%s,%s,%s,%s)",
                                           (db_iban, db_user_id, db_type, db_balance))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)

                        # Resultaat van Query voor het versturen van iban,type,saldo
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Nieuwe account voor gebruiker, gelukt!")
                                print("Rekening is aangemaakt!")
                            else:
                                print("Rekening is niet aangemaakt!")
                        except TypeError as e:
                            print(e)

                        # Query voor maken van de registratie
                        try:
                            print("[i] Account aan gebruiker koppelen!")
                            cursor.execute("INSERT INTO registrations (user_id,iban) VALUES (%s,%s)",
                                           (db_user_id, db_iban))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)

                        # Resultaat van Query voor het versturen van user_id, iban
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Account aan gebruiker koppelen, gelukt!")
                                print("Gebruikerid:", db_user_id, " met IBAN:", db_iban, "  is aangemaakt!")
                            else:
                                print("Gebruikerid:", db_user_id, " met IBAN:", db_iban, "  is niet aangemaakt!")
                        except TypeError as e:
                            print(e)

                        # Query voor maken van de bankpas
                        try:
                            db_card_id = ' '.join(format(ord(c), 'b') for c in db_card_id)
                            db_pin = ph.hash(str(db_pin))
                            print("[i] Kaart kopppelen aan account van gebruiker!")
                            cursor.execute("INSERT INTO cards (id,user_id,iban,pin) VALUES (%s,%s,%s,%s)",
                                           (db_card_id.replace(" ", ""), db_user_id, db_iban, db_pin))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)

                        # Resultaat van Query voor het versturen van card_id, iban, user_id, pin
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Kaart kopppelen aan account van gebruiker, gelukt!")
                                print("Card_id:\t", db_card_id, "\nUser_id:\t", db_user_id, "\nIBAN:\t", db_iban,
                                      "\nPin:\t", db_pin)
                            else:
                                print("[i] Kaart kopppelen aan account van gebruiker, mislukt!")
                        except TypeError as e:
                            print(e)

                    finally:
                        continue

                elif keuze == 2:
                    print("\n-[Gebruiker verwijderen]-")

                    try:
                        db_gebruikerid = int(input("Gebruikerid\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            # verwijder van de tabel gebruikers de regel waar id overeenkomt met de input van de gebruiker
                            cursor.execute("""DELETE FROM users WHERE id = '%s'""" % (db_gebruikerid))
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Gebruiker ", db_gebruikerid, " succesvol verwijderd!")
                            else:
                                print("[!] Gebruiker ", db_gebruikerid, " is niet verwijderd!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                elif keuze == 3:
                    print("\n-[Rekening openen]-")

                    try:
                        db_gebruikerid = int(input("GebruikerID\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        # bij onjuiste invoer vraag opnieuw naar input van gebruiker
                        continue

                    try:
                        db_iban = str(input("IBAN\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_type = int(input("Type\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_balance = float(input("Saldo\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            # voeg toe in tabel rekeningen de gebruikersid, rekeningnummer en saldo met de input van de gebruiker
                            sql = "INSERT INTO accounts (iban,user_id, type, balance) VALUES (%s, %s, %s, %s)"
                            values = (db_iban, db_gebruikerid, db_type, db_balance)
                            cursor.execute(sql, values)
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Rekening ", db_iban, " succesvol toegevoegd!")
                            else:
                                print("[!] Rekening ", db_iban, " is niet toegevoegd!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                elif keuze == 4:
                    print("\n-[Rekening saldo wijzigen]-")

                    try:
                        db_iban = str(input("IBAN\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_balance = float(input("Saldo\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            # verander in de tabel accounts de balans waar rij van ingevulde iban voorkomt in tabel
                            sql = "UPDATE accounts SET balance = %s WHERE iban = %s"
                            values = (db_balance, db_iban)
                            cursor.execute(sql, values)
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Saldo Rekening ", db_iban, " succesvol bijgewerkt!")
                            else:
                                print("[!] Saldo Rekening ", db_iban, " is niet bijgewerkt!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                elif keuze == 5:
                    print("\n-[Pas toevoegen]-")

                    try:
                        db_gebruikerid = int(input("GebruikerID\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        # bij onjuiste invoer vraag opnieuw naar input van gebruiker
                        continue

                    try:
                        db_iban = str(input("IBAN\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_card_id = str(input("Pasnummer\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_pin = int(input("Pincode\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            db_card_id = ' '.join(format(ord(c), 'b') for c in db_card_id)
                            db_pin = ph.hash(str(db_pin))
                            # voeg toe in de tabel pasjes de rekeningnummer, pasnummer en pincode met de input van de gebruiker
                            sql = "INSERT INTO cards (id, iban,user_id, pin) VALUES (%s,%s,%s,%s)"
                            values = (db_card_id.replace(" ", ""), db_iban, db_gebruikerid, db_pin)
                            cursor.execute(sql, values)
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Pas ", db_card_id.replace(" ", ""), " succesvol gekoppeld!")
                            else:
                                print("[!] Pas ", db_card_id.replace(" ", ""), " is niet gekoppeld!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                elif keuze == 6:
                    print("\n-[Pas blokkeren]-")

                    try:
                        db_gebruikerid = int(input("GebruikerID\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_iban = str(input("IBAN\t\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_card_id = str(input("Pasnummer\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            db_card_id = ' '.join(format(ord(c), 'b') for c in db_card_id)
                            sql = "UPDATE cards SET attempts = 3 WHERE user_id = %s and iban = %s and id = %s"
                            values = (db_gebruikerid, db_iban, db_card_id.replace(" ", ""))
                            cursor.execute(sql, values)
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Pas ", db_card_id.replace(" ", ""), " succesvol geblokkeerd!")
                            else:
                                print("[!] Pas ", db_card_id.replace(" ", ""), " is niet geblokkeerd!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                elif keuze == 7:
                    print("\n-[Pas Deblokkeren]-")

                    try:
                        db_iban = str(input("IBAN\t\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        db_card_id = str(input("Pasnummer\t: "))
                    except ValueError:
                        print("[!] Onjuiste invoer, probeer opnieuw")
                        continue

                    try:
                        try:
                            db_card_id = ' '.join(format(ord(c), 'b') for c in db_card_id)

                            sql = "UPDATE cards SET attempts = 0 WHERE iban = %s and id = %s"
                            values = (db_iban, db_card_id.replace(" ", ""))
                            cursor.execute(sql, values)
                        except (mysql.connector.Error, mysql.connector.Warning) as e:
                            print(e)
                        try:
                            results = cursor.fetchone()
                            mydb.commit()
                            if cursor.rowcount > 0:
                                print("[i] Pas ", db_card_id.replace(" ", ""), " succesvol gedeblokkeerd!")
                            else:
                                print("[!] Pas ", db_card_id.replace(" ", ""), " is niet gedeblokkeerd!")
                        except TypeError as e:
                            print(e)
                    finally:
                        continue

                # als gebruiker voor 9 heeft gekozen stopt het script door isRunning false te maken en uit de while loop te breken
                elif keuze == 9:
                    print("\n[i] Python script gestopt")
                    isRunning = False
                    break

    # als de informatie die is ingevuld niet klopt print dit uit als foutmelding
    except TypeError as e:
        print(e)
