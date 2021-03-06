%% Fannerl: Erlang Bindings to the Fast Artificial Neural Network Library (fann)
%% Copyright (C) 2015 Erik Axling (erik.axling@gmail.com)

%% This library is free software; you can redistribute it and/or
%% modify it under the terms of the GNU Lesser General Public
%% License as published by the Free Software Foundation; either
%% version 2.1 of the License, or (at your option) any later version.

%% This library is distributed in the hope that it will be useful,
%% but WITHOUT ANY WARRANTY; without even the implied warranty of
%% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
%% Lesser General Public License for more details.

%% You should have received a copy of the GNU Lesser General Public
%% License along with this library; if not, write to the Free Software
%% Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

%% The fannerl version of FANNs mushroom.c example
-module(mushroom).

-export([run/0]).

run() ->
    %% Need to start the port driver
    fannerl:start(),
    
    NumHiddenNeuron = 32,
    DesiredError = 0.0001,
    
    io:format("Reading train from file~n", []),
    %DatasetDir = code:lib_dir(fannerl, datasets),
    {ok, DatasetDir} = application:get_env(fannerl, fannerl_datasets),
    Train = fannerl:read_train_from_file(filename:join(DatasetDir, "mushroom.train")),
    TrainParams = fannerl:get_train_params(Train),
    
    io:format("Creating Network~n", []),
    NumInput = maps:get(num_input, TrainParams),
    NumOutput = maps:get(num_output, TrainParams),
    Network = fannerl:create({NumInput, NumHiddenNeuron, NumOutput}),
    io:format("Training Network~n", []),
    fannerl:set_activation_function_hidden(Network, fann_sigmoid_symmetric),
    fannerl:set_activation_function_output(Network, fann_sigmoid),
    ok = fannerl:train_on_data(Network, Train, 300, 10, DesiredError),

    io:format("Testing Network~n", []),
    Test = fannerl:read_train_from_file(filename:join(DatasetDir, "mushroom.train")),

    ok = fannerl:reset_mse(Network),

    {ok, Mse} = fannerl:test_data(Network, Test),
    
    io:format("MSE error on test data ~p~n", [Mse]),
    
    io:format("Saving Network~n", []),
    ok = fannerl:save(Network, "mushroom_float.net"),

    io:format("Cleaning Up~n", []),
    fannerl:destroy_train(Train),
    fannerl:destroy_train(Test),
    fannerl:destroy(Network),
    fannerl:stop().
