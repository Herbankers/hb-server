users			(**ID** : int, First name : string, Last name : string)

cards			(**ID** : int, \underline{User ID}[^1] : int, **Card ID** : int, PIN, Attempts : int)

accounts		(**ID** : int, \underline{User ID}[^1] : int, Type : int, IBAN : string, Balance)

transactions	(**ID** : int, Status : int, Source IBAN : string, Destination IBAN : string, Amount)

[^1]: Can't be NULL
