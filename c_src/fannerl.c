/*
  Fannerl: Erlang Bindings to the Fast Artificial Neural Network Library (fann)
  Copyright (C) 2015 Erik Axling (erik.axling@gmail.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <ei.h>

#include <unistd.h>

#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doublefann.h"
#include "uthash.h"
 
#define BUF_SIZE 128 
   
typedef unsigned char byte;

int read_cmd(byte **buf, int *size);
int write_cmd(ei_x_buff* x);
int read_exact(byte *buf, int len);
int write_exact(byte *buf, int len);
int error_with_reason(ei_x_buff * result, char * reason);
static inline int error_with_line(ei_x_buff * result, long unsigned line,
				  char * reason);

int do_fann_create_standard(byte *buf, int * index, ei_x_buff * result);
int do_fann_create_from_file(byte *buf, int * index, ei_x_buff * result);
int do_fann_copy(byte *buf, int * index, ei_x_buff * result);
int do_fann_destroy(byte *buf, int * index, ei_x_buff * result);
int do_fann_train(byte *buf, int * index, ei_x_buff * result);
int do_fann_run(byte *buf, int * index, ei_x_buff * result);
int do_fann_train_on_file(byte *buf, int * index, ei_x_buff * result);
int do_fann_get_param(byte*buf, int * index, ei_x_buff * result);
int do_fann_read_train_from_file(byte*buf, int * index, ei_x_buff * result);
int do_fann_destroy_train(byte *buf, int * index, ei_x_buff * result);
int do_fann_train_on_data(byte*buf, int * index, ei_x_buff * result);
int do_fann_train_epoch(byte*buf, int * index, ei_x_buff * result);
int do_fann_test_data(byte*buf, int * index, ei_x_buff * result);
int do_fann_test(byte*buf, int * index, ei_x_buff * result);
int do_fann_save_to_file(byte*buf, int * index, ei_x_buff * result);
int do_fann_shuffle_train(byte*buf, int * index, ei_x_buff * result);
int do_fann_subset_train_data(byte*buf, int * index, ei_x_buff * result);
int do_fann_randomize_weights(byte*buf, int * index, ei_x_buff * result);
int do_fann_init_weights(byte*buf, int * index, ei_x_buff * result);
int do_fann_scale_train(byte*buf, int * index, ei_x_buff * result);
int do_fann_descale_train(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_scaling_params(byte*buf, int * index, ei_x_buff * result);
int do_fann_clear_scaling_params(byte*buf, int * index, ei_x_buff * result);
int do_fann_reset_mse(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_weights(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_weight(byte*buf, int * index, ei_x_buff * result);
int do_fann_merge_trains(byte*buf, int * index, ei_x_buff * result);
int do_fann_duplicate_train(byte*buf, int * index, ei_x_buff * result);
int do_fann_save_train(byte*buf, int * index, ei_x_buff * result);
int do_fann_get_train_params(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_params(byte*buf, int * index, ei_x_buff * result);
int do_fann_get_activation_function(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_activation_function(byte*buf, int * index, ei_x_buff * result);
int do_fann_get_activation_steepness(byte*buf, int * index, ei_x_buff * result);
int do_fann_set_activation_steepness(byte*buf, int * index, ei_x_buff * result);
int do_fann_scale_input(byte*buf, int * index, ei_x_buff * result);
int do_fann_scale_output(byte*buf, int * index, ei_x_buff * result);
int do_fann_descale_input(byte*buf, int * index, ei_x_buff * result);
int do_fann_descale_output(byte*buf, int * index, ei_x_buff * result);

int get_tuple_double_data(byte * buf, int * index, double * inputs,
			  unsigned int num_inputs);
double get_double(byte * buf, int * index);
int get_fann_ptr(byte * buf, int * index, struct fann** fann);
int get_fann_train_ptr(byte * buf, int * index, struct fann_train_data** fann);

int get_activation_function(char * activation_function);
void get_activation_function_atom(enum fann_activationfunc_enum activation_function, char * act_func);

int get_param(byte*buf, int * index, struct fann * network,
	      char * param, ei_x_buff * result);

int set_params(byte * buf, int * index, struct fann * network);
int set_param(byte * buf, int * index, struct fann * network, char * param);

int is_atom(byte * buf, int * index);
int is_integer(byte * buf, int * index);
int is_string(byte * buf, int * index);
int is_float(byte * buf, int * index);
int get_strlen(byte * buf, int * index);

int FANN_API test_callback(struct fann *ann, struct fann_train_data *train,
                           unsigned int max_epochs,
			   unsigned int epochs_between_reports,
                           float desired_error, unsigned int epochs);

struct ann_map {
  int key;
  struct fann * ann;
  UT_hash_handle hh; // makes this structure hashable
};

struct train_map {
  int key;
  struct fann_train_data * train;
  UT_hash_handle hh; // makes this structure hashable
};

struct ann_map * ann_map = NULL;
struct train_map * train_map = NULL;

int ann_ctr=0;
int train_ctr=0;

void add_ann(int ann_num, struct fann * ann) {
  struct ann_map * s;
  HASH_FIND_INT(ann_map, &ann_num, s);
  if(s == NULL) {
    s = malloc(sizeof(struct ann_map));
    s->key = ann_num;
    s->ann = ann;
    HASH_ADD_INT(ann_map, key, s);
  }  
}

struct fann * find_ann(int ann_num) {
  struct ann_map * s;
  HASH_FIND_INT(ann_map, &ann_num, s);
  return s->ann;
}

void delete_ann(int ann_num) {
  struct ann_map * s;
  HASH_FIND_INT(ann_map, &ann_num, s);
  if(s != NULL) {
    HASH_DEL(ann_map, s);
    free(s);
  }
}

unsigned int num_ann() {
  return HASH_COUNT(ann_map);
}

void add_train(int train_num, struct fann_train_data * train) {
  struct train_map * s;
  HASH_FIND_INT(train_map, &train_num, s);
  if(s == NULL) {
    s = malloc(sizeof(struct train_map));
    s->key = train_num;
    s->train = train;
    HASH_ADD_INT(train_map, key, s);
  }  
}

struct train_map * find_train(int train_num) {
  struct train_map * s;
  HASH_FIND_INT(train_map, &train_num, s);
  return s;
}

void delete_train(int train_num) {
  struct train_map * s;
  HASH_FIND_INT(train_map, &train_num, s);
  if(s != NULL) {
    HASH_DEL(train_map, s);
    free(s);
  }
}

unsigned int num_train() {
  return HASH_COUNT(train_map);
}

/*-----------------------------------------------------------------
 * API functions
 *----------------------------------------------------------------*/

/*-----------------------------------------------------------------
 * MAIN
 *----------------------------------------------------------------*/
int main() {
  byte*     buf;
  int       size = BUF_SIZE;
  char      command[MAXATOMLEN];
  int       index, version, arity;

  ei_x_buff result;
 
  #ifdef _WIN32
  /* Attention Windows programmers: you need to explicitly set
   * mode of stdin/stdout to binary or else the port program won't work
   */
  setmode(fileno(stdout), O_BINARY);
  setmode(fileno(stdin), O_BINARY);
  #endif
 
  if ((buf = (byte *) malloc(size)) == NULL) 
    return -1;
     
  while (read_cmd(&buf, &size) > 0) {
    /* Reset the index, so that ei functions can decode terms from the 
     * beginning of the buffer */
    index = 0;
    /* Ensure that we are receiving the binary term by reading and
     * stripping the version byte */
    if (ei_decode_version((const char *)buf, &index, &version)) {
      return 199;
    }
    /* Our marshalling spec is that we are expecting a tuple
       {Command, Ref, Arg} or {create_standard, Arg} */
    if (ei_decode_tuple_header((const char *)buf, &index, &arity)) return 2;
     
    if (arity > 3 || arity < 2) return 3;
     
    if (ei_decode_atom((const char *)buf, &index, command)) return 4;
    // Prepare the output buffer that will hold {ok, Result} or {error, Reason}
    if (ei_x_new_with_version(&result) || ei_x_encode_tuple_header(&result, 2))
      return 5;
    if(!strcmp("create_standard", command)) {
      
      if(do_fann_create_standard(buf, &index, &result) != 1) return 9;
      
    } else if(!strcmp("create_from_file", command)) {
      
      if(do_fann_create_from_file(buf, &index, &result) != 1) return 23;
      
    } else if(!strcmp("copy", command)) {
      
      if(do_fann_copy(buf, &index, &result) != 1) return 26;
      
    } else if(!strcmp("destroy", command)) {

      if(do_fann_destroy(buf, &index, &result) != 1) return 10;

    } else if(!strcmp("train", command)) {

      if(do_fann_train(buf, &index, &result) != 1) return 13;

    } else if(!strcmp("run", command)) {

      if(do_fann_run(buf, &index, &result) != 1) return 14;

    } else if(!strcmp("train_on_file", command)) {

      if(do_fann_train_on_file(buf, &index, &result) != 1) return 15;

    } else if(!strcmp("get_param", command)) {

      if(do_fann_get_param(buf, &index, &result) != 1) return 16;
      
    } else if(!strcmp("read_train_from_file", command)) {

      if(do_fann_read_train_from_file(buf, &index, &result) != 1) return 17;

    } else if(!strcmp("destroy_train", command)) {

      if(do_fann_destroy_train(buf, &index, &result) != 1) return 17;

    } else if(!strcmp("train_on_data", command)) {

      if(do_fann_train_on_data(buf, &index, &result) != 1) return 18;

    } else if(!strcmp("train_epoch", command)) {

      if(do_fann_train_epoch(buf, &index, &result) != 1) return 19;

    } else if(!strcmp("test_data", command)) {

      if(do_fann_test_data(buf, &index, &result) != 1) return 20;

    } else if(!strcmp("test", command)) {

      if(do_fann_test(buf, &index, &result) != 1) return 21;

    } else if(!strcmp("save_to_file", command)) {

      if(do_fann_save_to_file(buf, &index, &result) != 1) return 22;

    } else if(!strcmp("shuffle_train", command)) {

      if(do_fann_shuffle_train(buf, &index, &result) != 1) return 24;

    } else if(!strcmp("subset_train_data", command)) {

      if(do_fann_subset_train_data(buf, &index, &result) != 1) return 25;

    } else if(!strcmp("randomize_weights", command)) {

      if(do_fann_randomize_weights(buf, &index, &result) != 1) return 29;

    } else if(!strcmp("init_weights", command)) {

      if(do_fann_init_weights(buf, &index, &result) != 1) return 30;

    } else if(!strcmp("scale_train", command)) {

      if(do_fann_scale_train(buf, &index, &result) != 1) return 31;

    } else if(!strcmp("descale_train", command)) {

      if(do_fann_descale_train(buf, &index, &result) != 1) return 32;

    } else if(!strcmp("set_scaling_params", command)) {

      if(do_fann_set_scaling_params(buf, &index, &result) != 1) return 33;

    } else if(!strcmp("clear_scaling_params", command)) {

      if(do_fann_clear_scaling_params(buf, &index, &result) != 1) return 34;

    } else if(!strcmp("reset_mse", command)) {

      if(do_fann_reset_mse(buf, &index, &result) != 1) return 35;

    } else if(!strcmp("set_weights", command)) {

      if(do_fann_set_weights(buf, &index, &result) != 1) return 36;

    } else if(!strcmp("set_weight", command)) {

      if(do_fann_set_weight(buf, &index, &result) != 1) return 37;

    } else if(!strcmp("merge_train", command)) {

      if(do_fann_merge_trains(buf, &index, &result) != 1) return 38;

    } else if(!strcmp("duplicate_train", command)) {

      if(do_fann_duplicate_train(buf, &index, &result) != 1) return 39;

    } else if(!strcmp("save_train", command)) {

      if(do_fann_save_train(buf, &index, &result) != 1) return 40;

    } else if(!strcmp("get_train_params", command)) {

      if(do_fann_get_train_params(buf, &index, &result) != 1) return 41;

    } else if(!strcmp("set_params", command)) {

      if(do_fann_set_params(buf, &index, &result) != 1) return 42;

    } else if(!strcmp("get_activation_function", command)) {
      
      if(do_fann_get_activation_function(buf, &index, &result) != 1) return 43;

    } else if(!strcmp("set_activation_function", command)) {
      
      if(do_fann_set_activation_function(buf, &index, &result) != 1) return 44;

    } else if(!strcmp("get_activation_steepness", command)) {
      
      if(do_fann_get_activation_steepness(buf, &index, &result) != 1) return 45;
      
    } else if(!strcmp("set_activation_steepness", command)) {
      
      if(do_fann_set_activation_steepness(buf, &index, &result) != 1) return 46;
      
    } else if(!strcmp("scale_input", command)) {
      
      if(do_fann_scale_input(buf, &index, &result) != 1) return 47;
      
    } else if(!strcmp("scale_output", command)) {
      
      if(do_fann_scale_output(buf, &index, &result) != 1) return 48;
      
    }  else if(!strcmp("descale_input", command)) {
      
      if(do_fann_descale_input(buf, &index, &result) != 1) return 49;
      
    } else if(!strcmp("descale_output", command)) {
      
      if(do_fann_descale_output(buf, &index, &result) != 1) return 50;
      
    } else {
      if (ei_x_encode_atom(&result, "error") ||
	  ei_x_encode_atom(&result, "unsupported_command")) 
        return 99;
    }
    write_cmd(&result);
 
    ei_x_free(&result);
  }
  return 0;
}

/*-----------------------------------------------------------------
 * API functions
 *----------------------------------------------------------------*/
int do_fann_create_standard(byte *buf, int * index, ei_x_buff * result) {
  int arity, size;
  char type[MAXATOMLEN];
  // decode {Network, Type, ConnRate, Options}
  if(ei_decode_tuple_header((const char*)buf, index, &size)) return -1;
  if(ei_decode_tuple_header((const char*)buf, index, &arity)) return -1;
  unsigned int layers[arity];
  unsigned long layerNum = 0;
  struct fann * network;
  // Here we dynamically create an array to use in fann_create_standard
  // arity is the same as num_layers
  for(int i = 0; i < arity; ++i) {
    if(ei_decode_ulong((const char *)buf, index, &layerNum)) return -1;
    layers[i] = (unsigned int)layerNum;
  }
  // Decode type
  if(ei_decode_atom((const char *)buf, index, type)) return -1;
  if(!strcmp("standard", type)) {
    network = fann_create_standard_array(arity, layers);
    ei_skip_term((const char*)buf, index);
  } else if(!strcmp("sparse", type)) {
    double temp_conn_rate;
    temp_conn_rate = get_double(buf, index);
    network = fann_create_sparse_array(temp_conn_rate, arity, layers);
  } else if(!strcmp("shortcut", type)) {
    network = fann_create_shortcut_array(arity, layers);
    ei_skip_term((const char*)buf, index);
  } else {
    // standard course of action
    network = fann_create_standard_array(arity, layers);
    ei_skip_term((const char*)buf, index);
  }
  if(set_params(buf, index, network) != 1) return -1;
  int hash_key = ann_ctr;
  add_ann(ann_ctr, network);
  ann_ctr += 1;
  if(ei_x_encode_atom(result, "ok") || ei_x_encode_long(result,
							(long)hash_key))
    return -1;
  return 1;
}

int do_fann_create_from_file(byte *buf, int * index, ei_x_buff * result) {
  struct fann * network = NULL;
  char * filename = NULL;
  int size;

  // Decode Filename
  if(is_string(buf, index)) {
    size = get_strlen(buf, index);
    filename = malloc((size+1)*sizeof(char));

    if(ei_decode_string((const char *)buf, index, filename)) return -1;

    network = fann_create_from_file(filename);
    if(network == NULL) return -1;
    
    int hash_key = ann_ctr;
    add_ann(ann_ctr, network);
    ann_ctr += 1;
    if(ei_x_encode_atom(result, "ok") || ei_x_encode_long(result,
							  (long)hash_key))
      return -1;
    return 1;
  } else {
    return -1;
  }

}

int do_fann_copy(byte *buf, int * index, ei_x_buff * result) {
  struct fann * oldNetwork = NULL;
  struct fann * newNetwork = NULL;

  if(get_fann_ptr(buf, index, &oldNetwork) != 1) return -1;
    
  newNetwork = fann_copy(oldNetwork);
  if(newNetwork == NULL) return -1;

  int hash_key = ann_ctr;
  add_ann(ann_ctr, newNetwork);
  ann_ctr += 1;
  if(ei_x_encode_atom(result, "ok") || ei_x_encode_long(result,
							(long)hash_key))
    return -1;
  return 1;

}

int do_fann_destroy(byte *buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  // decode Ptr, {} (skip empty tuple)
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  ei_skip_term((const char * )buf, index);
  fann_destroy(network);
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  return 1;
}

int do_fann_train(byte *buf, int * index, ei_x_buff * result) {
  unsigned int num_inputs, num_outputs;
  int arity;
  struct fann * network=0;

  // Decode Ptr,{Input, Output}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;

  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  num_inputs = fann_get_num_input(network);
  num_outputs = fann_get_num_output(network);
  double inputs[num_inputs];
  //Decode the inputs, verify size
  if(get_tuple_double_data(buf, index, inputs, num_inputs) == -1) return -1;
  double outputs[num_outputs];
  if(get_tuple_double_data(buf, index, outputs, num_outputs) == -1) return -1;

  fann_train(network, (fann_type*)inputs, (fann_type *) outputs);

  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  return 1;
}

int do_fann_train_on_file(byte *buf, int * index, ei_x_buff * result) {
  int arity;
  struct fann * network = 0;
  char * filename = 0;
  unsigned long max_epochs = 0;
  unsigned long epochs_between_report = 0;
  double desired_error = 0.0;
  int size;

  // Decode Ptr,( Filename, MaxEpochs, EpochBetweenReports, DesiredError}
  // Decode network ptr
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(is_string(buf, index)) {
    size = get_strlen(buf, index);
    filename = malloc((size+1)*sizeof(char));
    // Decode filename
    if(ei_decode_string((const char *)buf, index, filename)) return -1;
    
    //Decode MaxEpochs
    if(ei_decode_ulong((const char *)buf, index, &max_epochs)) return -1;
    
    //Decode EpochBetweenReports
    if(ei_decode_ulong((const char *)buf, index, &epochs_between_report))
      return -1;
  
    //Decode DesiredError
    if(ei_decode_double((const char *)buf, index, &desired_error)) return -1;

    fann_set_callback(network, test_callback);
    //Call FANN API
    fann_train_on_file(network, filename, (unsigned int)max_epochs,
		       epochs_between_report,
		       (float)desired_error);
    
    free(filename);
    fann_set_callback(network, NULL);
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
  } else {
    return 1;
  }
}

int do_fann_run(byte *buf, int * index, ei_x_buff * result) {
  unsigned int num_inputs, num_outputs;
  int arity;
  struct fann * network = 0;

  //Decode Ptr, {Input}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  num_inputs = fann_get_num_input(network);
  num_outputs = fann_get_num_output(network);
  
  double inputs[num_inputs];
  //Decode the inputs, verify size
  if(get_tuple_double_data(buf, index,
			   inputs, num_inputs) == -1) return -1;
  double * outputs;
  outputs = fann_run(network, inputs);
  
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, num_outputs)) return -1;
  
  for(int i=0; i < num_outputs; ++i) {
    ei_x_encode_double(result, outputs[i]);
  }
  return 1;
}

int do_fann_get_param(byte*buf, int * index, ei_x_buff * result)  {
  struct fann * network = 0;
  char param[MAXATOMLEN];
  int tuple_arity;
  
  
  //Decode Ptr, {Param}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;
  if(ei_decode_atom((const char *)buf, index, param)) return -1;

  if(get_param(buf, index, network, param, result) == -1) return -1;
  return 1;
}

int get_param(byte*buf, int * index, struct fann * network,
	      char * param, ei_x_buff * result) {
  float learning_rate, learning_momentum, mse, connection_rate;
  enum fann_train_enum train_alg;
  enum fann_errorfunc_enum error_func;
  enum fann_nettype_enum network_type;
  unsigned int bit_fail;
  unsigned int num_input, num_output, total_neurons;
  unsigned int total_connections;
  unsigned int num_layers;
  unsigned int * layers;
  unsigned int * bias;
  struct fann_connection * connections;
  enum fann_stopfunc_enum stop_func;
  float quickprop_decay, quickprop_mu, rprop_increase_factor;
  float rprop_decrease_factor, rprop_delta_min, rprop_delta_max;
  float rprop_delta_zero, sarprop_weight_decay_shift;
  float sarprop_step_error_threshold_factor;
  float sarprop_step_error_shift, sarprop_temperature;

  if(ei_x_new_with_version(result) == -1) return -1;

  if(!strcmp("learning_rate", param)) {
    learning_rate = fann_get_learning_rate(network);
    if(ei_x_encode_double(result, (double)learning_rate) != 0) return -1;
  } else if(!strcmp("learning_momentum", param)) {
    learning_momentum = fann_get_learning_momentum(network);
    if(ei_x_encode_double(result, (double)learning_momentum) != 0) return -1;
  } else if(!strcmp("training_algorithm", param)) {
    train_alg = fann_get_training_algorithm(network);
    
    if(train_alg == FANN_TRAIN_INCREMENTAL) {
      ei_x_encode_atom(result, "fann_train_incremental");
    } else if(train_alg == FANN_TRAIN_BATCH) {
      ei_x_encode_atom(result, "fann_train_batch");
    } else if(train_alg == FANN_TRAIN_RPROP) {
      ei_x_encode_atom(result, "fann_train_rprop");
    } else if(train_alg == FANN_TRAIN_QUICKPROP) {
      ei_x_encode_atom(result, "fann_train_quickprop");
    } else {
      ei_x_encode_atom(result, "undefined");
    }
  } else if(!strcmp("mean_square_error", param)) {
    mse = fann_get_MSE(network);
    if(ei_x_encode_double(result, (double)mse) != 0) return -1;
  } else if(!strcmp("bit_fail", param)) {
    bit_fail = fann_get_bit_fail(network);
    ei_x_encode_ulong(result, (unsigned long)bit_fail);
  } else if(!strcmp("train_error_function", param)) {
    error_func = fann_get_train_error_function(network);
    if(error_func == FANN_ERRORFUNC_LINEAR) {
      ei_x_encode_atom(result, "fann_errorfunc_linear");
    } else if(error_func == FANN_ERRORFUNC_TANH) {
      ei_x_encode_atom(result, "fann_errorfunc_tanh");
    } else {
      ei_x_encode_atom(result, "undefined");
    }
  } else if(!strcmp("network_type", param)) {
    network_type = fann_get_network_type(network);
    if(network_type == FANN_NETTYPE_LAYER) {
      ei_x_encode_atom(result, "fann_nettype_layer");
    } else if(network_type == FANN_NETTYPE_SHORTCUT) {
      ei_x_encode_atom(result, "fann_nettype_shortcut");
    } else {
      ei_x_encode_atom(result, "undefined");
    }
  } else if(!strcmp("num_input", param)) {
    num_input = fann_get_num_input(network);
    ei_x_encode_ulong(result, (unsigned long)num_input);
  } else if(!strcmp("num_output", param)) {
    num_output = fann_get_num_output(network);
    ei_x_encode_ulong(result, (unsigned long)num_output);
  } else if(!strcmp("total_neurons", param)) {
    total_neurons = fann_get_total_neurons(network);
    ei_x_encode_ulong(result, (unsigned long)total_neurons);
  } else if(!strcmp("total_connections", param)) {
    total_connections = fann_get_total_connections(network);
    ei_x_encode_ulong(result, (unsigned long)total_connections);
  } else if(!strcmp("connection_rate", param)) {
    connection_rate = fann_get_connection_rate(network);
    if(ei_x_encode_double(result, (double)connection_rate) != 0) return -1;
  } else if(!strcmp("num_layers", param)) {
    num_layers = fann_get_num_layers(network);
    ei_x_encode_ulong(result, (unsigned int)num_layers);
  } else if(!strcmp("train_stop_function", param)) {
    stop_func = fann_get_train_stop_function(network);
    if(stop_func == FANN_STOPFUNC_MSE) {
      ei_x_encode_atom(result, "fann_stopfunc_mse");
    } else {
      ei_x_encode_atom(result, "fann_stopfunc_bit");
    }
  } else if(!strcmp("quickprop_decay", param)) {
    quickprop_decay = fann_get_quickprop_decay(network);
    if(ei_x_encode_double(result, (double)quickprop_decay) != 0) return -1;
  } else if(!strcmp("quickprop_mu", param)) {
    quickprop_mu = fann_get_quickprop_mu(network);
    if(ei_x_encode_double(result, (double)quickprop_mu) != 0) return -1;
  } else if(!strcmp("rprop_increase_factor", param)) {
    rprop_increase_factor = fann_get_rprop_increase_factor(network);
    if(ei_x_encode_double(result, (double)  rprop_increase_factor) != 0)
      return -1;
  } else if(!strcmp("rprop_decrease_factor", param)) {
    rprop_decrease_factor = fann_get_rprop_decrease_factor(network);
    if(ei_x_encode_double(result, (double)  rprop_decrease_factor) != 0)
      return -1;
  } else if(!strcmp("rprop_delta_min", param)) {
    rprop_delta_min = fann_get_rprop_delta_min(network);
    if(ei_x_encode_double(result, (double)rprop_delta_min) != 0) return -1;
  } else if(!strcmp("rprop_delta_max", param)) {
    rprop_delta_max = fann_get_rprop_delta_max(network);
    if(ei_x_encode_double(result, (double)rprop_delta_max) != 0) return -1;
  } else if(!strcmp("rprop_delta_zero", param)) {
    rprop_delta_zero = fann_get_rprop_delta_zero(network);
    if(ei_x_encode_double(result, (double)rprop_delta_zero) != 0)
      return -1;
  } else if(!strcmp("sarprop_weight_decay_shift", param)) {
    sarprop_weight_decay_shift = fann_get_sarprop_weight_decay_shift(network);
    if(ei_x_encode_double(result, (double)sarprop_weight_decay_shift) != 0)
      return -1;
  } else if(!strcmp("sarprop_step_error_threshold_factor", param)) {
    sarprop_step_error_threshold_factor =
      fann_get_sarprop_step_error_threshold_factor(network);
    if(ei_x_encode_double(result,
			  (double)sarprop_step_error_threshold_factor) != 0)
      return -1;
  } else if(!strcmp("sarprop_step_error_shift", param)) {
    sarprop_step_error_shift = fann_get_sarprop_step_error_shift(network);
    if(ei_x_encode_double(result, (double)sarprop_step_error_shift) != 0)
      return -1;
  } else if(!strcmp("sarprop_temperature", param)) {
    sarprop_temperature = fann_get_sarprop_temperature(network);
    if(ei_x_encode_double(result, (double)sarprop_temperature) != 0) return -1;
  } else if(!strcmp("layers", param)) {
    num_layers = fann_get_num_layers(network);
    layers = (unsigned int * )malloc(sizeof(unsigned int)*num_layers);
    fann_get_layer_array(network, layers);
    ei_x_encode_tuple_header(result, num_layers);
    for(int i = 0; i < num_layers; ++i) {
      if(ei_x_encode_ulong(result, (unsigned int)layers[i]) == -1) return -1;
    }
    free(layers);
  } else if(!strcmp("bias", param)) {
    num_layers = fann_get_num_layers(network);
    bias = (unsigned int * )malloc(sizeof(unsigned int)*num_layers);
    fann_get_bias_array(network, bias);
    if(ei_x_encode_tuple_header(result, num_layers) != 0) return -1;
    for(int i = 0; i < num_layers; ++i) {
      ei_x_encode_ulong(result, (unsigned int)bias[i]);
    }
    free(bias);
  } else if(!strcmp("connections", param)) {
    total_connections = fann_get_total_connections(network);
    // connections, encode into map where {From, To} is the key
    connections = (struct fann_connection *)
      malloc(sizeof(struct fann_connection)*total_connections);
    fann_get_connection_array(network, connections);
    if(ei_x_encode_map_header(result, total_connections) != 0) return -1;
    for(int i = 0; i < total_connections; ++i) {
      // size will be 2 as {From, To}
      if(ei_x_encode_tuple_header(result, 2) != 0) return -1; 
      if(ei_x_encode_ulong(result, connections[i].from_neuron) != 0) return -1;
      if(ei_x_encode_ulong(result, connections[i].to_neuron) != 0) return -1;
      if(ei_x_encode_double(result, connections[i].weight) != 0) return -1;
    }
    free(connections);
  } else {
    if(ei_x_encode_tuple_header(result, 2) == -1) return -1;
    if(ei_x_encode_atom(result, "error") == -1) return -1;
    if(ei_x_encode_tuple_header(result, 2) == -1) return -1;
    if(ei_x_encode_atom(result, "unrecognized_param") == -1) return -1;
    if(ei_x_encode_atom(result, param) == -1) return -1;
  }
  return 1;
}

int do_fann_read_train_from_file(byte*buf, int * index, ei_x_buff * result)  {
  char * filename = 0;
  struct fann_train_data * training_data;
  int size;
  if(is_string(buf, index)) {
    size = get_strlen(buf, index);
    //Alloc memory for string
    filename = malloc((size +1) * sizeof(char));
    if(ei_decode_string((const char*)buf, index, filename)) return -1;
    training_data = fann_read_train_from_file(filename);
    free(filename);
    if(ei_x_new_with_version(result)) return -1;
    if(ei_x_encode_tuple_header(result, 2)) return -1;
    if(ei_x_encode_atom(result, "ok") ||
       ei_x_encode_long(result, (long)training_data))
      return -1;
    return 1; 
  } else {
    return -1;
  }
}

int do_fann_train_on_data(byte*buf, int * index, ei_x_buff * result) {
  int arityRefs, arityArgs;
  struct fann * network;
  struct fann_train_data * train_data;
  unsigned long max_epochs = 0;
  unsigned long epochs_between_report = 0;
  double desired_error;
  // Decode {NetworkRef, TrainRef}, {MaxEpochs, EpochBetweenReports, DesiredError}
  if(ei_decode_tuple_header((const char *)buf, index, &arityRefs)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);
  
  if(ei_decode_tuple_header((const char *)buf, index, &arityArgs)) return -1;
  
  if(ei_decode_ulong((const char*)buf, index, &max_epochs)) return -1;

  if(ei_decode_ulong((const char*)buf, index, &epochs_between_report))
    return -1;
  
  desired_error = get_double(buf, index);
  fann_set_callback(network, test_callback);
  fann_train_on_data(network, train_data, max_epochs,
		     epochs_between_report, desired_error);
  fann_set_callback(network, NULL);
  
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;  
  return 1;
}

int do_fann_train_epoch(byte*buf, int * index, ei_x_buff * result) {
  int arityRefs;
  struct fann * network;
  struct fann_train_data * train_data;
  // Decode {NetworkRef, TrainRef}, {MaxEpochs, DesiredError}
  if(ei_decode_tuple_header((const char *)buf, index, &arityRefs)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);

  fann_train_epoch(network, train_data);

  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;  
  return 1;
}

int do_fann_test_data(byte*buf, int * index, ei_x_buff * result) {
  int arityRefs;
  float MSE;
  struct fann * network;
  struct fann_train_data * train_data;
  // Decode {NetworkRef, TrainRef}, {}
  if(ei_decode_tuple_header((const char *)buf, index, &arityRefs)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);

  MSE = fann_test_data(network, train_data);
  
  if(ei_x_new_with_version(result)) return -1;
  if(ei_x_encode_tuple_header(result, 2)) return -1;
  if(ei_x_encode_atom(result, "ok") ||
     ei_x_encode_double(result, (double)MSE))
    return -1;
  return 1;
}

int do_fann_test(byte*buf, int * index, ei_x_buff * result) {
  unsigned int num_inputs, num_outputs;
  int arity;
  struct fann * network = 0;

  //Decode Ptr, {Input, DesiredOutput}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  num_inputs = fann_get_num_input(network);
  num_outputs = fann_get_num_output(network);
  
  double input[num_inputs];
  //Decode the inputs, verify size
  if(get_tuple_double_data(buf, index,
			   input, num_inputs) == -1) return -1;
  double desired_output[num_outputs];
  //Decode the desired_output, verify size
  if(get_tuple_double_data(buf, index,
			   desired_output, num_outputs) == -1) return -1;
  double * output;
  output = fann_test(network, input, desired_output);
  
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, num_outputs)) return -1;
  
  for(int i=0; i < num_outputs; ++i) {
    ei_x_encode_double(result, output[i]);
  }
  return 1;
}

int do_fann_save_to_file(byte*buf, int * index, ei_x_buff * result) {
  int arity;
  struct fann * network;
  char * filename = NULL;
  int size;
  // Decode Ptr, {Filename}
  // Decode network ptr
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(is_string(buf, index)) {
    size = get_strlen(buf, index);
    filename = malloc((size+1)*sizeof(char));
    // Decode filename
    if(ei_decode_string((const char *)buf, index, filename)) return -1;
    
    if(fann_save(network, filename) == -1) {
      free(filename);
      return -1;
    }

    free(filename);
    
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
  } else {
    return -1;
  }
}

int do_fann_save_train(byte*buf, int * index, ei_x_buff * result) {
  int arity;
  int size;
  struct fann_train_data * train;
  char * filename = NULL;
  // Decode TrainPtr, {Filename}
  // Decode train ptr
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(is_string(buf, index)) {
    size = get_strlen(buf, index);

    filename = malloc((size+1)*sizeof(char));
    // Decode filename
    if(ei_decode_string((const char *)buf, index, filename)) return -1;
    
    if(fann_save_train(train, filename) == -1) {
      free(filename);
      return -1;
    }

    free(filename);
    
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
  } else {
    return -1;
  }
}

int do_fann_shuffle_train(byte*buf, int * index, ei_x_buff * result) {
  struct fann_train_data * train;
  // Decode trainPtr, {}
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;

  // Skip the {} part
  ei_skip_term((const char*)buf, index);

  fann_shuffle_train_data(train);
  
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  
  return 1;
}

int do_fann_subset_train_data(byte*buf, int * index, ei_x_buff * result) {
  struct fann_train_data * train;
  struct fann_train_data * copy;
  unsigned long pos, length;
  int arity;
  // Decode trainPtr, {Pos, Length}
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;

  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(ei_decode_ulong((const char *)buf, index, &pos)) return -1;
  if(ei_decode_ulong((const char *)buf, index, &length)) return -1;

  copy = fann_subset_train_data(train, pos, length);

  if(ei_x_new_with_version(result)) return -1;
  if(ei_x_encode_tuple_header(result, 2)) return -1;
  if(ei_x_encode_atom(result, "ok") ||
     ei_x_encode_long(result, (long)copy))
    return -1;
  return 1;
}

int do_fann_destroy_train(byte*buf, int * index, ei_x_buff * result) {
  struct fann_train_data * train;

  // Decode trainPtr, {}
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;

  fann_destroy_train(train);

  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_randomize_weights(byte*buf, int * index, ei_x_buff * result) {
  fann_type min_weight, max_weight;
  struct fann * network = 0;
  int arity;
    
  // Decode Ptr, {MinWeight, MaxWeight}
  // Decode network ptr
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
    
  //Decode MinWeight
  min_weight = get_double(buf, index);
  max_weight = get_double(buf, index);
  
  fann_randomize_weights(network, min_weight, max_weight);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_init_weights(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network;
  struct fann_train_data * train_data;
  int arity;

  // Decode {NetworkRef, TrainRef}, {}
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);
  
  fann_init_weights(network, train_data);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_scale_train(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network;
  struct fann_train_data * train_data;
  int arity;

  // Decode {NetworkRef, TrainRef}, {}
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) 
    return error_with_line(result, __LINE__, "Error decoding tuple");

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);
  
  fann_scale_train(network, train_data);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_descale_train(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network;
  struct fann_train_data * train_data;
  int arity;

  // Decode {NetworkRef, TrainRef}, {}
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);
  
  fann_descale_train(network, train_data);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_set_scaling_params(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network;
  struct fann_train_data * train_data;
  int arity, arity_data;
  double input_min, input_max, output_min, output_max;

  // Decode {NetworkRef, TrainRef},
  // {NewInputMin, NewInputMax, NewOutputMin, NewOutputMax}
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;

  get_fann_ptr(buf, index, &network);
  get_fann_train_ptr(buf, index, &train_data);

  if(ei_decode_tuple_header((const char *)buf, index, &arity_data)) return -1;
  if(ei_decode_double((const char *)buf, index, &input_min)) return -1;
  if(ei_decode_double((const char *)buf, index, &input_max)) return -1;
  if(ei_decode_double((const char *)buf, index, &output_min)) return -1;
  if(ei_decode_double((const char *)buf, index, &output_max)) return -1;
  
  
  fann_set_scaling_params(network, train_data,
			  input_min, input_max, output_min, output_max);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_clear_scaling_params(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;

  //Decode Ptr, {}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  ei_skip_term((const char*)buf, index);

  fann_clear_scaling_params(network);
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_reset_mse(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  //Decode Ptr, {}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  
  fann_reset_MSE(network);

  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;

  return 1;
}

int do_fann_set_weights(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int arity, connections_to_set, key_arity;
  struct fann_connection * connections;
  int type, type_size, real_size = 0;
  unsigned long from_neuron, to_neuron;
  //Decode Ptr, {Connections}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;

  // Decode header
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  if(ei_decode_map_header((const char *)buf, index, &connections_to_set))
    return -1;
  if(connections_to_set > 0) { // If < 0 do nothing and return ok
    // We now know length of connection array so alloc memory
    connections = malloc(sizeof(struct fann_connection)*connections_to_set);
  
    // Expect maps to be defined like {FromNeuron, ToNeureon} => Weight
    // if not, then ignore
    for(int i = 0; i < connections_to_set; ++i) {
      ei_get_type((const char *)buf, index, &type, &type_size);
      if((type == ERL_SMALL_TUPLE_EXT || type == ERL_LARGE_TUPLE_EXT) &&
	 type_size == 2) {
	// We got a tuple with size 2, lets fetch the weight value
	if(ei_decode_tuple_header((const char *)buf, index, &key_arity))
	  return -1;
	if(ei_decode_ulong((const char *)buf, index, &from_neuron)) return -1;
	if(ei_decode_ulong((const char *)buf, index, &to_neuron)) return -1;
	connections[i].weight = get_double(buf, index);
	connections[i].from_neuron = from_neuron;
	connections[i].to_neuron = to_neuron;
	real_size += 1;
      }
    }
    fann_set_weight_array(network, connections, real_size);
    free(connections);
  }
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  
  return 1;
}


int do_fann_set_weight(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int arity;
  unsigned long from_neuron, to_neuron;
  fann_type weight;
  //Decode Ptr, {FromNeuron, ToNeuron, Weight}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;

  // Decode header
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  if(ei_decode_ulong((const char *)buf, index, &from_neuron)) return -1;
  if(ei_decode_ulong((const char *)buf, index, &to_neuron)) return -1;
  weight = (fann_type)get_double(buf, index);
  fann_set_weight(network, from_neuron, to_neuron, weight);

  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  
  return 1;
}

int do_fann_merge_trains(byte*buf, int * index, ei_x_buff * result) {
  struct fann_train_data * train1;
  struct fann_train_data * train2;
  struct fann_train_data * new_train;
  int arity;
  
  //Decode {Train1, Train2}, {}
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  if(get_fann_train_ptr(buf, index, &train1) != 1) return -1;
  if(get_fann_train_ptr(buf, index, &train2) != 1) return -1;
  // Skip the {} part
  ei_skip_term((const char*)buf, index);

  new_train = fann_merge_train_data(train1, train2);
  
  if(ei_x_new_with_version(result)) return -1;
  if(ei_x_encode_tuple_header(result, 2)) return -1;
  
  if(ei_x_encode_atom(result, "ok") ||
     ei_x_encode_long(result, (long)new_train))
    return -1;
  return 1;
}

int do_fann_duplicate_train(byte*buf, int * index, ei_x_buff * result) {
  struct fann_train_data * train;
  struct fann_train_data * new_train;
  //Decode Train, {}
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;
  // Skip the {} part
  ei_skip_term((const char*)buf, index);
  new_train = fann_duplicate_train_data(train);
  
  if(ei_x_new_with_version(result)) return -1;
  if(ei_x_encode_tuple_header(result, 2)) return -1;
  if(ei_x_encode_atom(result, "ok") ||
     ei_x_encode_long(result, (long)new_train))
    return -1;
  return 1;
}

int do_fann_get_train_params(byte*buf, int * index, ei_x_buff * result)  {
  struct fann_train_data * train;
  unsigned int length, num_input, num_output;
    
  //Decode Train, {}
  if(get_fann_train_ptr(buf, index, &train) != 1) return -1;
  // Skip the {} part
  ei_skip_term((const char*)buf, index);

  // Fetch all params
  length = fann_length_train_data(train);
  num_input = fann_num_input_train_data(train);
  num_output = fann_num_output_train_data(train);

  if(ei_x_new_with_version(result)) return -1;
  if(ei_x_encode_map_header(result, 3)) return -1;
  
  if(ei_x_encode_atom(result, "length")) return -1;
  if(ei_x_encode_ulong(result, length)) return -1;
  
  if(ei_x_encode_atom(result, "num_input")) return -1;
  if(ei_x_encode_ulong(result, num_input)) return -1;

  if(ei_x_encode_atom(result, "num_output")) return -1;
  if(ei_x_encode_ulong(result, num_output)) return -1;
  return 1;
}

int do_fann_set_params(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int tuple_arity;

  //Decode Ptr, {ParamMap}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;

  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;
  
  if(set_params(buf, index, network) != 1) return -1 ;
    
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  
  return 1;
}

int do_fann_get_activation_function(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int tuple_arity;
  char * activation_function = 0;

  unsigned long layer, neuron;
  enum fann_activationfunc_enum act_func;
  //Decode Ptr, {Layer, Neuron}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;

  if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &layer)) return -1;
    
    // ok get the neuron
    if(is_integer(buf, index)) {
      if(ei_decode_ulong((const char *)buf, index, &neuron)) return -1;
      // Ok we have both layer and neuron. get the activation function
      act_func = fann_get_activation_function(network, layer, neuron);
      
      activation_function = malloc(MAXATOMLEN);
      get_activation_function_atom(act_func, activation_function);
      if(ei_x_new_with_version(result) ||
	 ei_x_encode_atom_len(result, activation_function,
			      strlen(activation_function))) return -1;
      free(activation_function);
      return 1;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int do_fann_set_activation_function(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int tuple_arity;
  char activation_function[MAXATOMLEN];
  char layeratom[MAXATOMLEN];
  char neuronatom[MAXATOMLEN];
  unsigned long layer, neuron;
  enum fann_activationfunc_enum act_func;
  //Decode Ptr, {ActivationFunction, Layer, Neuron}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;
  
  if(ei_decode_atom((const char *)buf, index, activation_function)) return -1;
  act_func = get_activation_function(activation_function);
  
  // Layer can be all, hidden, output or positive integer > 0
  if(is_atom(buf, index)) {
    if(ei_decode_atom((const char *)buf, index, layeratom)) return -1;
    if(!strcmp("hidden", layeratom)) {
      fann_set_activation_function_hidden(network, act_func);
    } else if(!strcmp("output", layeratom)) {
      fann_set_activation_function_output(network, act_func);
    } else if(!strcmp("all",layeratom)) {
      fann_set_activation_function_hidden(network, act_func);
      fann_set_activation_function_output(network, act_func);
    } else {
      return -1;
    }
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
    
  } else if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &layer)) return -1;
  } else {
    return -1;
  }
  
  // get neuron, can be all or positive integer >= 0
  if(is_atom(buf, index)) {
    if(ei_decode_atom((const char *)buf, index, neuronatom)) return -1;
    if(!strcmp("all", neuronatom)) {
      fann_set_activation_function_layer(network, act_func, layer);
    } else {
      return -1;
    }
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
  } else if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &neuron)) return -1;
    fann_set_activation_function(network, act_func, layer, neuron);
  } else {
    return -1;
  }
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  return 1;
}

int do_fann_get_activation_steepness(byte*buf, int * index, ei_x_buff * result){
  struct fann * network = 0;
  int tuple_arity;

  unsigned long layer, neuron;
  fann_type steepness;
  //Decode Ptr, {Layer, Neuron}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;


  if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &layer)) return -1;
    
    // ok get the neuron
    if(is_integer(buf, index)) {
      if(ei_decode_ulong((const char *)buf, index, &neuron)) return -1;
      // Ok we have both layer and neuron. get the activation steepness
      steepness = fann_get_activation_steepness(network, layer, neuron);
      
      if(ei_x_new_with_version(result) ||
	 ei_x_encode_double(result, steepness)) return -1;

      return 1;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int do_fann_set_activation_steepness(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  int tuple_arity;
 
  char layeratom[MAXATOMLEN];
  char neuronatom[MAXATOMLEN];
  unsigned long layer, neuron;
  fann_type steepness;
  //Decode Ptr, {Steepness, Layer, Neuron}
  // Decode network ptr first
  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;
  
  steepness = (fann_type)get_double(buf, index);

  // Layer can be all, hidden, output or positive integer > 0
  if(is_atom(buf, index)) {
    if(ei_decode_atom((const char *)buf, index, layeratom)) return -1;
    if(!strcmp("hidden", layeratom)) {
      fann_set_activation_steepness_hidden(network, steepness);
    } else if(!strcmp("output", layeratom)) {
      fann_set_activation_steepness_output(network, steepness);
    } else if(!strcmp("all",layeratom)) {
      fann_set_activation_steepness_hidden(network, steepness);
      fann_set_activation_steepness_output(network, steepness);
    } else {
      return -1;
    }
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
    
  } else if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &layer)) return -1;
  } else {
    return -1;
  }
  
  // get neuron, can be all or positive integer >= 0
  if(is_atom(buf, index)) {
    if(ei_decode_atom((const char *)buf, index, neuronatom)) return -1;
    if(!strcmp("all", neuronatom)) {
      fann_set_activation_steepness_layer(network, steepness, layer);
    } else {
      return -1;
    }
    if(ei_x_new_with_version(result) ||
       ei_x_encode_atom_len(result, "ok", 2)) return -1;
    return 1;
  } else if(is_integer(buf, index)) {
    if(ei_decode_ulong((const char *)buf, index, &neuron)) return -1;
    fann_set_activation_steepness(network, steepness, layer, neuron);
  } else {
    return -1;
  }
  if(ei_x_new_with_version(result) ||
     ei_x_encode_atom_len(result, "ok", 2)) return -1;
  return 1;
}

int do_fann_scale_output(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  fann_type * output = 0;
  
  int arity, tuple_arity;
  // Decode network, {Output}

  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;

  // Sanity check output length to see that it is not larger than output layer
  if(fann_get_num_output(network) != tuple_arity) {
    return error_with_line(result, __LINE__,
			   "Output size not same as number of output neurons");
  }
  //allocate the output vector
  output = malloc(tuple_arity*sizeof(fann_type));
  for(int i = 0; i < tuple_arity; ++i) {
    output[i] = (fann_type)get_double(buf, index);
  }
  fann_scale_output(network, output);
  // Ok now we need to return the tuple
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, tuple_arity)) 
    return -1;
  for(int i = 0; i < tuple_arity; ++i) {
    if(ei_x_encode_double(result, (double)output[i]) != 0) return -1;
  }
  free(output);
  return 1;
}

int do_fann_descale_input(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  fann_type * input = 0;
  
  int arity, tuple_arity;
  // Decode network, {Input}

  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;

  // Sanity check input length to see that it is not larger than input layer
  if(fann_get_num_input(network) != tuple_arity) {
    return error_with_line(result, __LINE__,
			   "Input size not same as number of input neurons");
  }
  //allocate the input vector
  input = malloc(tuple_arity*sizeof(fann_type));
  for(int i = 0; i < tuple_arity; ++i) {
    input[i] = (fann_type)get_double(buf, index);
  }
  fann_descale_input(network, input);
  // Ok now we need to return the tuple
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, tuple_arity)) 
    return -1;
  for(int i = 0; i < tuple_arity; ++i) {
    if(ei_x_encode_double(result, (double)input[i]) != 0) return -1;
  }
  free(input);
  return 1;
}

int do_fann_descale_output(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  fann_type * output = 0;
  
  int arity, tuple_arity;
  // Decode network, {Output}

  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;

  // Sanity check output length to see that it is not larger than output layer
  if(fann_get_num_output(network) != tuple_arity) {
    return error_with_line(result, __LINE__,
			   "Output size not same as number of output neurons");
  }
  //allocate the output vector
  output = malloc(tuple_arity*sizeof(fann_type));
  for(int i = 0; i < tuple_arity; ++i) {
    output[i] = (fann_type)get_double(buf, index);
  }
  fann_descale_output(network, output);
  // Ok now we need to return the tuple
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, tuple_arity)) 
    return -1;
  for(int i = 0; i < tuple_arity; ++i) {
    if(ei_x_encode_double(result, (double)output[i]) != 0) return -1;
  }
  free(output);
  return 1;
}

int do_fann_scale_input(byte*buf, int * index, ei_x_buff * result) {
  struct fann * network = 0;
  fann_type * input = 0;
  
  int arity, tuple_arity;
  // Decode network, {Input}

  if(get_fann_ptr(buf, index, &network) != 1) return -1;
  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  
  if(ei_decode_tuple_header((const char *)buf, index, &tuple_arity)) return -1;

  // Sanity check input length to see that it is not larger than input layer
  if(fann_get_num_input(network) != tuple_arity) {
    return error_with_line(result, __LINE__,
			   "Input size not same as number of input neurons");
  }
  //allocate the input vector
  input = malloc(tuple_arity*sizeof(fann_type));
  for(int i = 0; i < tuple_arity; ++i) {
    input[i] = (fann_type)get_double(buf, index);
  }
  fann_scale_input(network, input);
  // Ok now we need to return the tuple
  if(ei_x_new_with_version(result) ||
     ei_x_encode_tuple_header(result, tuple_arity)) 
    return -1;
  for(int i = 0; i < tuple_arity; ++i) {
    if(ei_x_encode_double(result, (double)input[i]) != 0) return -1;
  }
  free(input);
  return 1;
}

/*-----------------------------------------------------------------
 * Util functions
 *----------------------------------------------------------------*/
int get_tuple_double_data(byte * buf, int * index, double * data,
			  unsigned int size) {
  int arity;

  if(ei_decode_tuple_header((const char *)buf, index, &arity)) return -1;
  if(arity != size) return -1;
  
  for(int i = 0; i < arity; ++i) {
    data[i] = get_double(buf, index);
  }
  return 1;
}

double get_double(byte*buf, int * index) {
  double double_value;
  long long_value;
  if(is_integer(buf, index)) {
    ei_decode_long((const char *)buf, index, &long_value);
    return (double)long_value;
  } else {
    ei_decode_double((const char *)buf, index, &double_value);
    return double_value;
  }
    
}

int get_fann_ptr(byte * buf, int * index, struct fann ** fann) {
  // Decode the network ptr
  unsigned long hash_key = 0;
  if(ei_decode_ulong((const char *)buf, index, &hash_key)) return -1;
  *fann = find_ann((int)hash_key);
  return 1;
}
int get_fann_train_ptr(byte * buf, int * index,
		       struct fann_train_data ** fann_train_data) {
  // Decode the training data ptr
  unsigned long ptr = 0;
  if(ei_decode_ulong((const char *)buf, index, &ptr)) return -1;
  *fann_train_data = (struct fann_train_data *)ptr;
  return 1;
}
 
int is_atom(byte * buf, int * index) {
  int type, type_size;
  ei_get_type((const char *)buf, index, &type, &type_size);
  return(type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT ||
	 type == ERL_ATOM_UTF8_EXT || type == ERL_SMALL_ATOM_UTF8_EXT);
}

int is_integer(byte * buf, int * index) {
  int type, type_size;
  ei_get_type((const char *)buf, index, &type, &type_size);
  return(type == ERL_SMALL_INTEGER_EXT || type == ERL_INTEGER_EXT);
}

int is_string(byte * buf, int * index) {
  int type, type_size;
  ei_get_type((const char *)buf, index, &type, &type_size);
  return(type == ERL_STRING_EXT);
}

int is_float(byte * buf, int * index) {
  int type, type_size;
  ei_get_type((const char *)buf, index, &type, &type_size);
  return(type == ERL_FLOAT_EXT || type == NEW_FLOAT_EXT);
}

int get_strlen(byte * buf, int * index) {
  int type, type_size;
  ei_get_type((const char *)buf, index, &type, &type_size);
  if(type == ERL_STRING_EXT) {
    return type_size;
  } else {
    return -1;
  }
}

static inline int error_with_line(ei_x_buff * result, long unsigned line,
				  char * reason) {
  char _reason[MAXATOMLEN];						
  sprintf(_reason, "fannerl.c:%lu: %s", line, reason);
  return error_with_reason(result, _reason);
}

int error_with_reason(ei_x_buff * result, char * reason) {
  int length = strlen(reason);
  if (ei_x_new_with_version(result) || ei_x_encode_tuple_header(result, 2)) 
    return -1;
  if (ei_x_encode_atom(result, "error") ||
      ei_x_encode_atom_len(result, reason, length))
    return -1;
  return 1;
}

int get_activation_function(char * activation_function) {

  if(!strcmp("fann_linear", activation_function)) {
    return FANN_LINEAR;
  } else if(!strcmp("fann_threshold", activation_function)) {
    return FANN_THRESHOLD;
  } else if(!strcmp("fann_threshold_symmetric", activation_function)) {
    return FANN_THRESHOLD_SYMMETRIC;
  } else if(!strcmp("fann_sigmoid", activation_function)) {
    return FANN_SIGMOID;
  } else if(!strcmp("fann_sigmoid_symmetric", activation_function)) {
    return FANN_SIGMOID_SYMMETRIC;
  } else if(!strcmp("fann_sigmoid_stepwise", activation_function)) {
    return FANN_SIGMOID_STEPWISE;
  } else if(!strcmp("fann_gaussian", activation_function)) {
    return FANN_GAUSSIAN;
  } else if(!strcmp("fann_gaussian_symmetric", activation_function)) {
    return FANN_GAUSSIAN_SYMMETRIC;
  } else if(!strcmp("fann_elliot", activation_function)) {
    return FANN_ELLIOT;
  } else if(!strcmp("fann_elliot_symmetric", activation_function)) {
    return FANN_ELLIOT_SYMMETRIC;
  } else if(!strcmp("fann_linear_piece", activation_function)) {
    return FANN_LINEAR_PIECE;
  } else if(!strcmp("fann_linear_piece_symmetric", activation_function)) {
    return FANN_LINEAR_PIECE_SYMMETRIC;
  } else if(!strcmp("fann_sin_symmetric", activation_function)) {
    return FANN_SIN_SYMMETRIC;
  } else if(!strcmp("fann_cos_symmetric", activation_function)) {
    return FANN_COS_SYMMETRIC;
  } else if(!strcmp("fann_sin", activation_function)) {
    return FANN_SIN;
  } else if(!strcmp("fann_cos", activation_function)) {
    return FANN_COS;
  } else {
    return -1;
  }
}

void get_activation_function_atom(enum fann_activationfunc_enum activation_function,
				    char * act_func) {

  if(activation_function == FANN_LINEAR) {
    strcpy(act_func, "fann_linear");
  } else if(activation_function == FANN_THRESHOLD) {
    strcpy(act_func, "fann_threshold");
  } else if(activation_function == FANN_THRESHOLD_SYMMETRIC) {
    strcpy(act_func, "fann_threshold_symmetric");
  } else if(activation_function == FANN_SIGMOID) {
    strcpy(act_func, "fann_sigmoid");
  } else if(activation_function == FANN_SIGMOID_SYMMETRIC) {
    strcpy(act_func, "fann_sigmoid_symmetric");
  } else if(activation_function == FANN_SIGMOID_STEPWISE) {
    strcpy(act_func, "fann_sigmoid_stepwise");
  } else if(activation_function == FANN_GAUSSIAN) {
    strcpy(act_func, "fann_gaussian");
  } else if(activation_function == FANN_GAUSSIAN_SYMMETRIC) {
    strcpy(act_func, "fann_gaussian_symmetric");
  } else if(activation_function == FANN_ELLIOT) {
    strcpy(act_func, "fann_elliot");
  } else if(activation_function == FANN_ELLIOT_SYMMETRIC) {
    strcpy(act_func, "fann_elliot_symmetric");
  } else if(activation_function == FANN_LINEAR_PIECE_SYMMETRIC) {
    strcpy(act_func, "fann_linear_piece_symmetric");
  } else if(activation_function == FANN_LINEAR_PIECE) {
    strcpy(act_func, "fann_linear_piece");
  } else if(activation_function == FANN_SIN_SYMMETRIC) {
    strcpy(act_func, "fann_sin_symmetric");
  } else if(activation_function == FANN_COS_SYMMETRIC) {
    strcpy(act_func, "fann_cos_symmetric");
  } else if(activation_function == FANN_COS) {
    strcpy(act_func, "fann_cos");
  } else if(activation_function == FANN_SIN) {
    strcpy(act_func, "fann_sin");
  } else {
    strcpy(act_func, "unknown_activation_function");
  }
}

int set_params(byte * buf, int * index, struct fann * network) {
  char param[MAXATOMLEN];
  int map_arity;
  if(ei_decode_map_header((const char *)buf, index, &map_arity)) return -1;
  for(int i = 0; i < map_arity; ++i) {
    if(is_atom(buf, index))  {
      if(ei_decode_atom((const char *)buf, index, param)) return -1;
      set_param(buf, index, network, param);
    } else {
      // Skip the two next ones
      ei_skip_term((const char*)buf, index);
      ei_skip_term((const char*)buf, index);
    }
  }
  return 1;
}

int set_param(byte * buf, int * index, struct fann * network, char * param) {
  double learning_rate, learning_momentum, quickprop_decay;
  double quickprop_mu, rprop_increase_factor, rprop_decrease_factor;
  double rprop_delta_min, rprop_delta_max, rprop_delta_zero;
  double sarprop_weight_decay_shift, sarprop_step_error_threshold_factor;
  double sarprop_step_error_shift, sarprop_temperature;
  fann_type bit_fail_limit;
  int type, type_size;
  char train_error_func[MAXATOMLEN];
  char train_stop_func[MAXATOMLEN];
  char training_algorithm[MAXATOMLEN];
  if(!strcmp("learning_rate", param)) {
    learning_rate = get_double(buf, index);
    fann_set_learning_rate(network, (float)learning_rate);
  } else if (!strcmp("learning_momentum", param)) {
    learning_momentum = get_double(buf, index);
    fann_set_learning_momentum(network, (float)learning_momentum);
  } else if (!strcmp("quickprop_decay", param)) {
    quickprop_decay = get_double(buf, index);
    fann_set_quickprop_decay(network, (float)quickprop_decay);
  } else if (!strcmp("quickprop_mu", param)) {
    quickprop_mu = get_double(buf, index);
    fann_set_quickprop_mu(network, (float)quickprop_mu);
  } else if (!strcmp("rprop_increase_factor", param)) {
    rprop_increase_factor = get_double(buf, index);
    fann_set_rprop_increase_factor(network, (float)rprop_increase_factor);
  } else if (!strcmp("rprop_decrease_factor", param)) {
    rprop_decrease_factor = get_double(buf, index);
    fann_set_rprop_decrease_factor(network, (float)rprop_decrease_factor);
  } else if (!strcmp("rprop_delta_min", param)) {
    rprop_delta_min = get_double(buf, index);
    fann_set_rprop_delta_min(network, (float)rprop_delta_min);
  } else if (!strcmp("rprop_delta_max", param)) {
    rprop_delta_max = get_double(buf, index);
    fann_set_rprop_delta_max(network, (float)rprop_delta_max);
  } else if (!strcmp("rprop_delta_zero", param)) {
    rprop_delta_zero = get_double(buf, index);
    fann_set_rprop_delta_zero(network, (float)rprop_delta_zero);
  } else if (!strcmp("sarprop_weight_decay_shift", param)) {
    sarprop_weight_decay_shift = get_double(buf, index);
    fann_set_sarprop_weight_decay_shift(network,
					(float)sarprop_weight_decay_shift);
  } else if (!strcmp("sarprop_step_error_threshold_factor", param)) {
    sarprop_step_error_threshold_factor = get_double(buf, index);
    fann_set_sarprop_step_error_threshold_factor(network,
						 (float)sarprop_step_error_threshold_factor);
  } else if (!strcmp("sarprop_step_error_shift", param)) {
    sarprop_step_error_shift = get_double(buf, index);
    fann_set_sarprop_step_error_shift(network,
				      (float)sarprop_step_error_shift);
  } else if (!strcmp("sarprop_temperature", param)) {
    sarprop_temperature = get_double(buf, index);
    fann_set_sarprop_temperature(network,
				 (float)sarprop_temperature);
  } else if (!strcmp("bit_fail_limit", param)) {
    bit_fail_limit = (fann_type)get_double(buf, index);
    fann_set_bit_fail_limit(network,
			    (float)bit_fail_limit);
  } else if (!strcmp("train_error_function", param)) {
    ei_get_type((const char *)buf, index, &type, &type_size);
    if(type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT ||
       type == ERL_ATOM_UTF8_EXT || type == ERL_SMALL_ATOM_UTF8_EXT)  {
      
      if(ei_decode_atom((const char *)buf, index, train_error_func)) return -1;
      
      if(!strcmp("fann_errorfunc_linear", train_error_func)) {
	fann_set_train_error_function(network, FANN_ERRORFUNC_LINEAR);
      } else if(!strcmp("fann_errorfunc_tanh", train_error_func)) {
	fann_set_train_error_function(network, FANN_ERRORFUNC_TANH);
      }
    } else {
      ei_skip_term((const char*)buf, index);
    }
  } else if (!strcmp("train_stop_function", param)) {
    ei_get_type((const char *)buf, index, &type, &type_size);
    if(type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT ||
       type == ERL_ATOM_UTF8_EXT || type == ERL_SMALL_ATOM_UTF8_EXT)  {
      
      if(ei_decode_atom((const char *)buf, index, train_stop_func)) return -1;
      
      if(!strcmp("fann_stopfunc_mse", train_stop_func)) {
	fann_set_train_stop_function(network, FANN_STOPFUNC_MSE);
      } else if(!strcmp("fann_stopfunc_tanh", train_stop_func)) {
	fann_set_train_stop_function(network, FANN_STOPFUNC_BIT);
      }
    } else {
      ei_skip_term((const char*)buf, index);
    }
  } else if (!strcmp("training_algorithm", param)) {
    ei_get_type((const char *)buf, index, &type, &type_size);
    if(type == ERL_ATOM_EXT || type == ERL_SMALL_ATOM_EXT ||
       type == ERL_ATOM_UTF8_EXT || type == ERL_SMALL_ATOM_UTF8_EXT)  {
      
      if(ei_decode_atom((const char *)buf, index, training_algorithm))
	return -1;
      
      if(!strcmp("fann_train_incremental", training_algorithm)) {
	fann_set_training_algorithm(network, FANN_TRAIN_INCREMENTAL);
      } else if(!strcmp("fann_train_batch", training_algorithm)) {
	fann_set_training_algorithm(network, FANN_TRAIN_BATCH);
      } else if(!strcmp("fann_train_rprop", training_algorithm)) {
	fann_set_training_algorithm(network, FANN_TRAIN_RPROP);
      } else if(!strcmp("fann_train_quickprop", training_algorithm)) {
	fann_set_training_algorithm(network, FANN_TRAIN_QUICKPROP);
      } else if(!strcmp("fann_train_sarprop", training_algorithm)) {
	fann_set_training_algorithm(network, FANN_TRAIN_SARPROP);
      }
    } else {
      ei_skip_term((const char*)buf, index);
    }
  }
  return 1;
}

int FANN_API test_callback(struct fann *ann, struct fann_train_data *train,
                           unsigned int max_epochs,
			   unsigned int epochs_between_reports,
                           float desired_error, unsigned int epochs)
{
   printf("Epochs     %8d. MSE: %.5f. Desired-MSE: %.5f\r\n", epochs, fann_get_MSE(ann), desired_error);
   return 0;
}

/*-----------------------------------------------------------------
 * Data marshalling functions
 *----------------------------------------------------------------*/
int read_cmd(byte **buf, int *size)
{
  int len;
 
  if (read_exact(*buf, 2) != 2)
    return(-1);
  len = ((*buf)[0] << 8) | (*buf)[1];
  
  if (len > *size) {
    byte* tmp = (byte *) realloc(*buf, len);
    if (tmp == NULL)
      return -1;
    else
      *buf = tmp;
    *size = len;
  }
  return read_exact(*buf, len);
}
 
int write_cmd(ei_x_buff *buff)
{
  byte li;
 
  li = (buff->index >> 8) & 0xff; 
  write_exact(&li, 1);
  li = buff->index & 0xff;
  write_exact(&li, 1);
 
  return write_exact((byte*)buff->buff, buff->index);
}
 
int read_exact(byte *buf, int len)
{
  int i, got=0;
 
  do {
    if ((i = read(3, buf+got, len-got)) <= 0)
      return i;
    got += i;
  } while (got<len);
 
  return len;
}
 
int write_exact(byte *buf, int len)
{
  int i, wrote = 0;
 
  do {
    if ((i = write(4, buf+wrote, len-wrote)) <= 0)
      return i;
    wrote += i;
  } while (wrote<len);
 
  return len;
}

