#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AccountService::AccountService() : HttpService("/users") {
  
}

// returns user email and balance
void AccountService::get(HTTPRequest *request, HTTPResponse *response) {
    // authenticate user
    User *user_obj = getAuthenticatedUser(request);

    // return error if user_id not passed in pararms
    if (request->getPathComponents().size() != 2){
        throw ClientError::badRequest();
    }
    // get user_id from parameters and make sure it matches with authenticated user's id
    string param_userid = request->getPathComponents()[1];
    cout << param_userid << endl;
    if (param_userid.empty()){
        throw ClientError::badRequest();
    }
    if (param_userid != user_obj->user_id){
        throw ClientError::forbidden();
    }

    // create response object
    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add email and balance
    o.AddMember("email", user_obj->email, a);
    o.AddMember("balance", user_obj->balance, a);

    // convert the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}

// updates user email address
void AccountService::put(HTTPRequest *request, HTTPResponse *response) {
    // check if email argument is provided
    WwwFormEncodedDict body = request->formEncodedBody();
    string email = body.get("email");

    if (email.empty()){
        throw ClientError::badRequest();
    }

    // authenticate and fetch user object
    User *user_obj = getAuthenticatedUser(request);

    // return error if user_id not passed in pararms
    if (request->getPathComponents().size() != 2){
        throw ClientError::badRequest();
    }
    // get user_id from parameters and make sure it matches with authenticated user's id
    string param_userid = request->getPathComponents()[1];
    if (param_userid.empty()){
        throw ClientError::badRequest();
    }
    if (param_userid != user_obj->user_id){
        throw ClientError::forbidden();
    }

    // set email
    user_obj->email = email;

    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add email and balance
    o.AddMember("email", user_obj->email, a);
    o.AddMember("balance", user_obj->balance, a);

    // convert the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
