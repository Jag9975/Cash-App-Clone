#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AuthService::AuthService() : HttpService("/auth-tokens") {
  
}

// create new user if username doesn't exist else log in if password matches
void AuthService::post(HTTPRequest *request, HTTPResponse *response) {
    WwwFormEncodedDict body = request->formEncodedBody();

    string username = body.get("username");
    string password = body.get("password");

    // check if both username and password are not empty
    if (username.empty() or password.empty()){
        throw ClientError::badRequest();
    }
    
    // make sure that username is all lowercase
    for (char i : username){
        if (!islower(i)){
            throw ClientError::badRequest();
        }
    }

    string auth_token;

    // if username and password match entry in database, return it's id and create auth_token
    if (this->m_db->users.find(username) != this->m_db->users.end()){
        // check if password matches
        if(this->m_db->users[username]->password == password){
            // create auth_token and store it in database
            auth_token = StringUtils::createAuthToken();
            this->m_db->auth_tokens[auth_token] = this->m_db->users[username];
        } else {
            throw ClientError::forbidden();
        }
    }
    // else create a new user entry in database and return it's id and create auth_token
    else {
        User *new_user = new User();
        new_user->username = username;
        new_user->password = password;
        new_user->balance = 0;

        // create a user_id and store it
        string user_id = StringUtils::createUserId();
        new_user->user_id = user_id;

        // save the user in database
        this->m_db->users[username] = new_user;
        
        // create auth_token and store it in database
        auth_token = StringUtils::createAuthToken();
        this->m_db->auth_tokens[auth_token] = new_user;

        // set status to 201 since we created new user entry in database
        response->setStatus(201);
    }

    // create return object
    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add id and token
    o.AddMember("auth_token", auth_token, a);
    o.AddMember("user_id", this->m_db->users[username]->user_id, a);

    // convert the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n")); 
}

void AuthService::del(HTTPRequest *request, HTTPResponse *response) {

    // get the user
    User *user_obj = this->getAuthenticatedUser(request);

    string token_for_deletion = request->getPathComponents()[1];

    // make sure auth_token requested for deletion belongs to the same user
    if (this->m_db->auth_tokens[token_for_deletion] == user_obj){
        this->m_db->auth_tokens.erase(token_for_deletion);
    } else {
        throw ClientError::forbidden();
    }
}
