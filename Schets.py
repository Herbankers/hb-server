import mysql.connector

db = mysql.connector.connect(
	host= "localhost",
	user= "root",
	passwd = "pass",
	database= "db"	
	)

mycursor = db.cursor()

var = raw_input("Voeg nieuwe gebruiker toe: ")
print "you entered", var

mycursor.execute("INSERT INTO users(user_id,first_name,last_name)VALUES(s%,s%,s%) 
