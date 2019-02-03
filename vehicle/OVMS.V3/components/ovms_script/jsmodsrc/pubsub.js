/**
 * Project:  Open Vehicle Monitor System
 * Date:     14th Jan 2019
 * Based on: https://github.com/mroderick/PubSubJS
 * Copyright (c) 2010,2011,2012,2013,2014 Morgan Roderick http://roderick.dk
 * License: MIT - http://mrgnrdrck.mit-license.org
 */

'use strict';

var messages = {},
    lastUid = -1;

function hasKeys(obj)
  {
  var key;
  for (key in obj)
    {
    if ( obj.hasOwnProperty(key) )
      {
      return true;
      }
    }
  return false;
  }

function callSubscriberWithImmediateExceptions( subscriber, message, data )
  {
  subscriber( message, data );
  }

function deliverMessage( originalMessage, matchedMessage, data )
  {
  var subscribers = messages[matchedMessage],
      s;

  if ( !messages.hasOwnProperty( matchedMessage ) )
    {
    return;
    }

  for (s in subscribers)
    {
    if ( subscribers.hasOwnProperty(s))
      {
      callSubscriberWithImmediateExceptions( subscribers[s], originalMessage, data );
      }
    }
  }

function createDeliveryFunction( message, data )
  {
  return function deliverNamespaced()
    {
    var topic = String( message ),
        position = topic.lastIndexOf( '.' );
    // deliver the message as it is now
    deliverMessage(message, message, data);
      // trim the hierarchy and deliver message to each level
    while( position !== -1 )
      {
      topic = topic.substr( 0, position );
      position = topic.lastIndexOf('.');
      deliverMessage( message, topic, data );
      }
    };
  }

function messageHasSubscribers( message )
  {
  var topic = String( message ),
      found = Boolean(messages.hasOwnProperty( topic ) && hasKeys(messages[topic])),
      position = topic.lastIndexOf( '.' );

  while ( !found && position !== -1 )
    {
    topic = topic.substr( 0, position );
    position = topic.lastIndexOf( '.' );
    found = Boolean(messages.hasOwnProperty( topic ) && hasKeys(messages[topic]));
    }

  return found;
  }

function publish( message, data )
  {
  message = (typeof message === 'symbol') ? message.toString() : message;

  var deliver = createDeliveryFunction( message, data ),
      hasSubscribers = messageHasSubscribers( message );

  if ( !hasSubscribers )
    {
    return false;
    }

  deliver();
  return true;
  }

/**
 * Publishes the message, passing the data to it's subscribers
 * @function
 * @alias publish
 * @param { String } message The message to publish
 * @param {} data The data to pass to subscribers
 * @return { Boolean }
 */
exports.publish = function( message, data )
  {
  return publish( message, data );
  };

/**
 * Subscribes the passed function to the passed message. Every returned token is unique and should be stored if you need to unsubscribe
 * @function
 * @alias subscribe
 * @param { String } message The message to subscribe to
 * @param { Function } func The function to call when a new message is published
 * @return { String }
 */
exports.subscribe = function( message, func )
  {
  if ( typeof func !== 'function')
    {
    return false;
    }

  message = (typeof message === 'symbol') ? message.toString() : message;

  // message is not registered yet
  if ( !messages.hasOwnProperty( message ) )
    {
    messages[message] = {};
    }

  // forcing token as String, to allow for future expansions without breaking usage
  // and allow for easy use as key names for the 'messages' object
  var token = 'uid_' + String(++lastUid);
  messages[message][token] = func;

  // return token for unsubscribing
  return token;
  };

/**
 * Clears all subscriptions
 * @function
 * @public
 * @alias clearAllSubscriptions
 */
exports.clearAllSubscriptions = function clearAllSubscriptions()
  {
  messages = {};
  };

/**
 * Clear subscriptions by the topic
 * @function
 * @public
 * @alias clearAllSubscriptions
 */
exports.clearSubscriptions = function clearSubscriptions(topic)
  {
  var m;
  for (m in messages)
    {
    if (messages.hasOwnProperty(m) && m.indexOf(topic) === 0)
      {
      delete messages[m];
      }
    }
  };

/**
 * Removes subscriptions
 *
 * - When passed a token, removes a specific subscription.
 *
 * - When passed a function, removes all subscriptions for that function
 *
 * - When passed a topic, removes all subscriptions for that topic (hierarchy)
 * @function
 * @public
 * @alias subscribeOnce
 * @param { String | Function } value A token, function or topic to unsubscribe from
 * @example // Unsubscribing with a token
 * var token = PubSub.subscribe('mytopic', myFunc);
 * PubSub.unsubscribe(token);
 * @example // Unsubscribing with a function
 * PubSub.unsubscribe(myFunc);
 * @example // Unsubscribing from a topic
 * PubSub.unsubscribe('mytopic');
 */
exports.unsubscribe = function(value)
  {
  var descendantTopicExists = function(topic)
    {
    var m;
    for ( m in messages )
      {
      if ( messages.hasOwnProperty(m) && m.indexOf(topic) === 0 )
        {
        // a descendant of the topic exists:
        return true;
        }
      }

    return false;
    },
    isTopic    = typeof value === 'string' && ( messages.hasOwnProperty(value) || descendantTopicExists(value) ),
    isToken    = !isTopic && typeof value === 'string',
    isFunction = typeof value === 'function',
    result = false,
    m, message, t;

  if (isTopic)
    {
    exports.clearSubscriptions(value);
    return;
    }

  for ( m in messages )
    {
    if ( messages.hasOwnProperty( m ) )
      {
      message = messages[m];

      if ( isToken && message[value] )
        {
        delete message[value];
        result = value;
        // tokens are unique, so we can just stop here
        break;
        }

      if (isFunction)
        {
        for ( t in message )
          {
          if (message.hasOwnProperty(t) && message[t] === value)
            {
            delete message[t];
            result = true;
            }
          }
        }
      }
    }

  return result;
  };
