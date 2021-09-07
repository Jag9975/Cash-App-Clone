#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "WwwFormEncodedDict.h"
#include "HttpClient.h"

#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

int API_SERVER_PORT = 8080;
string API_SERVER_HOST = "localhost";
string PUBLISHABLE_KEY = "";

string auth_token;
string user_id;

// convert the balance from cents to dollars
float cents_to_dollar(int cents){
  int x = cents/100;
  float y = (cents % 100)/100.0;   
  y += x;
  return y;
}

// convert the balance from dollars to cents
int dollar_to_cents(float dollars){
  dollars *= 100;
  return (int)dollars;
}

// break up the string arg by spaces
vector<string> split_by_space(string arg){
  string delimiter = " ";

  size_t pos = 0;
  vector<string> arg_broken;
  while ((pos = arg.find(delimiter)) != string::npos) {
      string to_push = arg.substr(0, pos);
      if(!to_push.empty()){
          arg_broken.push_back(to_push);
      }
      arg.erase(0, pos + delimiter.length());
  }

  if(!arg.empty()){
      arg_broken.push_back(arg);
  }

  return arg_broken;
}

// fetch user balance and display it
int balance(){
  // make GET request to /users/{user_id} endpoint
  HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
  client.set_header("x-auth-token", auth_token);
  string endpoint = "/users/" + user_id;
  HTTPClientResponse *client_response = client.get(endpoint);

  // return -1 if request failed, else print the balance and return 1
  if (!client_response->success()){
      return -1;
  } else {
    // convert the response body into a rapidjson document
    Document *res = client_response->jsonBody();

    int amount_charged = (*res)["balance"].GetInt();
    printf("Balance: $%.2f\n", cents_to_dollar(amount_charged));
    return 1;
  }
}

// handle user login
int auth(vector<string> params){
  // parse and check if all arguments are provided
  if (params.size() != 4){
    return -1;
  }

  // make an auth request to server
  HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
  WwwFormEncodedDict body;
  body.set("username", params[1]);
  body.set("password", params[2]);
  HTTPClientResponse *client_response = client.post("/auth-tokens", body.encode());

  // return -1 if request failed, else store the response, update email and return 1
  if (!client_response->success()){
      return -1;
  } else {
    // convert the response body into a rapidjson document
    Document *res = client_response->jsonBody();

    auth_token = (*res)["auth_token"].GetString();
    user_id = (*res)["user_id"].GetString();

    // make PUT request to /users/{user_id} endpoint to update email
    HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
    client.set_header("x-auth-token", auth_token);
    string endpoint = "/users/" + user_id;
    WwwFormEncodedDict body;
    body.set("email", params[3]);
    HTTPClientResponse *client_response = client.put(endpoint, body.encode());

    // return -1 if request failed, else return 1
    if (!client_response->success()){
        return -1;
    } else {
      // convert the response body into a rapidjson document
      Document *res = client_response->jsonBody();

      int amount_charged = (*res)["balance"].GetInt();
      printf("Balance: $%.2f\n", cents_to_dollar(amount_charged));
      return 1;
    }
  }
}

// handle user balance deposit
int deposit(vector<string> params){
  // parse and check if all arguments are provided
  if (params.size() != 6) {
    return -1;
  }
  int amount = dollar_to_cents(stof(params[1]));
  if (amount < 50) {
    return -1;
  }
  string card_num = params[2];
  string exp_year = params[3];
  string exp_month = params[4];
  string cvc = params[5];

  // call stripe API to convert credit card info into a token 
  HttpClient client("api.stripe.com", 443, true);
  client.set_header("Authorization", string("Bearer ") + PUBLISHABLE_KEY);

  // stripe call body arguments
  WwwFormEncodedDict body;
  body.set("card[number]", card_num);
  body.set("card[exp_year]", exp_year);
  body.set("card[exp_month]", exp_month);
  body.set("card[cvc]", cvc);
  HTTPClientResponse *client_response = client.post("/v1/tokens", body.encode());

  // return error if stripe call failed, else call server with token and amount
  if (!client_response->success()){
    return -1;
  } else {
    // convert the response body into a rapidjson document
    Document *res = client_response->jsonBody();
    string card_token = (*res)["id"].GetString();
    
    // call POST /deposits endpoint with token and amount
    HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
    client.set_header("x-auth-token", auth_token);
    WwwFormEncodedDict body;
    body.set("amount", amount);
    body.set("stripe_token", card_token);
    HTTPClientResponse *client_response = client.post("/deposits", body.encode());

    // return -1 if failed, else print balance and return 
    if (!client_response->success()){
        return -1;
    } else {
      // convert the response body into a rapidjson document
      Document *res = client_response->jsonBody();

      int amount_charged = (*res)["balance"].GetInt();
      printf("Balance: $%.2f\n", cents_to_dollar(amount_charged));
      return 1;
    }
  }
}

// sends money to another user
int send(vector<string> params){
  // make POST /deposits request to server
  HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
  client.set_header("x-auth-token", auth_token);
  WwwFormEncodedDict body;
  body.set("to", params[1]);
  body.set("amount", dollar_to_cents(stof(params[2])));
  HTTPClientResponse *client_response = client.post("/transfers", body.encode());

  // return -1 if request failed, else return 1
  if (!client_response->success()){
      return -1;
  } else {
    // convert the response body into a rapidjson document
    Document *res = client_response->jsonBody();

    int amount_charged = (*res)["balance"].GetInt();
    printf("Balance: $%.2f\n", cents_to_dollar(amount_charged));
    return 1;
  }
}

// delete the auth token from server and exit the program
int logout(){
  // make a DELETE /auth-tokens/{auth_token} request to server
  HttpClient client(API_SERVER_HOST.c_str(), API_SERVER_PORT, false);
  client.set_header("x-auth-token", auth_token);
  string endpoint = "/auth-tokens/" + auth_token;
  HTTPClientResponse *client_response = client.del(endpoint);

  // exit on successful request else print error
  if (!client_response->success()){
      return -1;
  } else {
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  stringstream config;
  int fd = open("config.json", O_RDONLY);
  if (fd < 0) {
    cout << "could not open config.json" << endl;
    exit(1);
  }
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    config << string(buffer, ret);
  }
  Document d;
  d.Parse(config.str());
  API_SERVER_PORT = d["api_server_port"].GetInt();
  API_SERVER_HOST = d["api_server_host"].GetString();
  PUBLISHABLE_KEY = d["stripe_publishable_key"].GetString();

  ifstream *file;
  ifstream open_file;

  string error = "Error";

  // exit if incorrect number of arguments provided
  if (argc > 2){
      cout << "Usage: ./dcash <optional batch file>" << endl;
      exit(1);
  }

  // set file pointer to either the batch file or stdin
  if(argc == 2){
      open_file.open(argv[1]);
      if(!open_file.is_open()){
          cout << "Error opening batch file" << endl;
          exit(1);
      }
      file = &open_file;
  } else {
      file = (ifstream*)&cin;
      cout << "D$> ";
  }

  string arg; // stores the return string from getline

  while(getline(*file, arg)){
    int status = 0;
    if (arg.empty()){
      cout << error << endl;
      if (argc == 1){
        cout << "D$> ";
      }
    } else {
      // parse the input
      vector<string> args_vec = split_by_space(arg);

      if (args_vec[0] == "auth"){
        status = auth(args_vec);
      } else if (args_vec[0] == "balance") {
        status = balance();
      } else if (args_vec[0] == "deposit") {
        status = deposit(args_vec);
      } else if (args_vec[0] == "send") {
        status = send(args_vec);
      } else if (args_vec[0] == "logout") {
        status = logout();
      } else {
        status = -1;
      }

      // print message if error encountered 
      if (status == -1){
        cout << error << endl;
      } 

      if (argc == 1){
        cout << "D$> ";
      }
    }
  }
  
  return 0;
}
