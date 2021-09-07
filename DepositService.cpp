#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") { }

// deposits money into account using the given token 
void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
    // check if user is authenticated 
    User *user_obj = getAuthenticatedUser(request);

    // extract arguments from request body
    WwwFormEncodedDict req_body = request->formEncodedBody();
    string amount = req_body.get("amount");
    string card_token = req_body.get("stripe_token");

    // make sure amount and token are provided
    if (amount.empty() || card_token.empty()){
        throw ClientError::badRequest();
    }

    // return error if less than 50 cents to charge requested
    if (stoi(amount) < 50){
        throw ClientError::badRequest();
    }

    HttpClient client("api.stripe.com", 443, true);
    client.set_basic_auth(m_db->stripe_secret_key, "");

    // create stripe API request body
    WwwFormEncodedDict body;
    body.set("currency", "usd");
    body.set("source", card_token);
    body.set("amount", amount);
    string encoded_body = body.encode();

    // make POST request
    HTTPClientResponse *client_response = client.post("/v1/charges", body.encode());

    // return error if stripe charge failed
    if (!client_response->success()){
        throw ClientError::badRequest();
    }

    // convert the response body into a rapidjson document
    Document *stripe_res = client_response->jsonBody();

    // extract amount and stripe_charge_id from stripe response body
    int amount_charged = (*stripe_res)["amount"].GetInt();
    string stripe_charge_id = (*stripe_res)["id"].GetString();

    // append this transaction to deposit array in database
    Deposit *transaction = new Deposit();
    transaction->to = user_obj;
    transaction->amount = amount_charged;
    transaction->stripe_charge_id = stripe_charge_id;
    this->m_db->deposits.push_back(transaction);

    // charge was successfull, add money to user's account
    user_obj->balance = user_obj->balance + amount_charged;

    // create response object
    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add balance to the response object
    o.AddMember("balance", user_obj->balance, a);

    // add all deposit transactions for this user to response object
    Value array;
    array.SetArray();
    for (Deposit* trans : this->m_db->deposits){
        // add the transaction object to our array if it belongs to the authenticated user
        if (trans->to == user_obj){
            Value to;
            to.SetObject();
            to.AddMember("to", trans->to->username, a);
            to.AddMember("amount", trans->amount, a);
            to.AddMember("stripe_charge_id", trans->stripe_charge_id, a);
            array.PushBack(to, a);
        }
    }

    // add the array to our return object
    o.AddMember("deposits", array, a);

    // convert the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
