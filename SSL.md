# Server CA

The server CA is the CA you will have to distribute across ATMs to ensure them
that the server they're connecting to can be trusted.

First, create a new password protected root key:
```
openssl genrsa -des3 -out ca.key 4096
```

Then, sign a new root CA to install onto Kech Bank ATMs:
```
openssl req -x509 -new -nodes -key ca.key -sha256 -out ca.pem
```

Now, create a new key for the server itself:
```
openssl genrsa -des3 -out server.key 4096
```

Generate a certificate signing request, the common name __must__ match the IP of
the server:
```
openssl req -new -key server.key -out server.csr
```

Finally, sign the new certificate:
```
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -out server.crt -sha256
```

The server can now be started with the following command line arguments:
```
kech-server -c server.crt -k server.key
```

# Client CA

The client CA should reside on the server at all times. This CA is used to sign
all the client certificates of the ATMs. This way, only ATMs signed by the
client CA will be able to access the server.

First, on the server, create a new password protected root key:
```
openssl genrsa -des3 -out clientca.key 4096
```

Then, sign a new root CA:
```
openssl req -x509 -new -nodes -key clientca.key -sha256 -out clientca.pem
```

## For every ATM

Create a new key for ATM:
```
openssl genrsa -des3 -out client.key 4096
```

Generate a certificate signing request, the common name __must__ match the IP of
the ATM:
```
openssl req -new -key client.key -out client.csr
```

Finally, sign the new certificate with the CA on the server:
```
openssl x509 -req -in client.csr -CA clientca.pem -CAkey clientca.key -out client.crt -sha256
```

The ATM can now be started with the following command line arguments:
```
kech-atm -c client.crt -k client.key
```
