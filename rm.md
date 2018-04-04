users			(**ID** : int, First name : string, Last name : string)

cards			(**ID** : int, \underline{User ID}[^1] : int, **Card ID** : int, PIN, Attempts : int)

accounts		(**IBAN** : string, , \underline{User ID}[^1] : int, Type : int, Balance)

transactions	(**ID** : int, Status : int, Timestamp : binary, Source IBAN : string, Destination IBAN : string, Amount)

[^1]: Can't be NULL
