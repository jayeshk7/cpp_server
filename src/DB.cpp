#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

PGconn* connectDB() {
  const char *conninfo = "dbname=postgres user=jayesh_decs password=jayesh@decs host=localhost port=5432";

  PGconn *conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK) {
    // If not successful, print the error message and finish the connection
    printf("Error while connecting to the database server: %s\n", PQerrorMessage(conn));

    PQfinish(conn);
    exit(1);
  }

  printf("Connection Established\n");
  // Close the connection and free the memory
  // PQfinish(conn);

  return conn;
}

