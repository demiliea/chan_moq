#!/usr/bin/env python3
"""
WebSocket Signaling Server for MoQ Channel Driver
Bridges between web clients and Asterisk chan_moq
"""

import asyncio
import json
import logging
from typing import Dict, Set
import websockets
from websockets.server import WebSocketServerProtocol

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Connected clients and sessions
clients: Set[WebSocketServerProtocol] = set()
sessions: Dict[str, dict] = {}

async def handle_client(websocket: WebSocketServerProtocol, path: str):
    """Handle WebSocket client connection"""
    client_id = f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
    logger.info(f"Client connected: {client_id}")
    
    clients.add(websocket)
    
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                msg_type = data.get('type')
                
                logger.info(f"Received message type '{msg_type}' from {client_id}")
                
                if msg_type == 'register':
                    # Client registers with an identifier
                    user_id = data.get('user_id')
                    if user_id:
                        sessions[user_id] = {
                            'websocket': websocket,
                            'user_id': user_id,
                            'status': 'available'
                        }
                        await websocket.send(json.dumps({
                            'type': 'registered',
                            'user_id': user_id,
                            'status': 'success'
                        }))
                        logger.info(f"User registered: {user_id}")
                
                elif msg_type == 'call':
                    # Initiate outgoing call
                    session_id = data.get('session_id')
                    dest = data.get('dest')
                    from_user = data.get('from')
                    
                    if dest in sessions:
                        # Route call to destination user
                        dest_ws = sessions[dest]['websocket']
                        await dest_ws.send(json.dumps({
                            'type': 'incoming_call',
                            'session_id': session_id,
                            'from': from_user
                        }))
                        logger.info(f"Call routed: {from_user} -> {dest}")
                        
                        # Send ringing to caller
                        await websocket.send(json.dumps({
                            'type': 'ringing',
                            'session_id': session_id
                        }))
                    else:
                        # Destination not found
                        await websocket.send(json.dumps({
                            'type': 'call_failed',
                            'session_id': session_id,
                            'reason': 'user_not_found'
                        }))
                        logger.warning(f"Call failed: destination {dest} not found")
                
                elif msg_type == 'answer':
                    # Answer incoming call
                    session_id = data.get('session_id')
                    
                    # Notify other party that call was answered
                    for user_id, session in sessions.items():
                        if session['websocket'] != websocket:
                            await session['websocket'].send(json.dumps({
                                'type': 'call_answered',
                                'session_id': session_id
                            }))
                    
                    logger.info(f"Call answered: session {session_id}")
                
                elif msg_type == 'hangup':
                    # Hangup call
                    session_id = data.get('session_id')
                    
                    # Notify all parties
                    for user_id, session in sessions.items():
                        try:
                            await session['websocket'].send(json.dumps({
                                'type': 'call_ended',
                                'session_id': session_id
                            }))
                        except:
                            pass
                    
                    logger.info(f"Call ended: session {session_id}")
                
                elif msg_type == 'sdp_offer' or msg_type == 'sdp_answer' or msg_type == 'ice_candidate':
                    # Forward WebRTC signaling messages
                    dest = data.get('dest')
                    if dest and dest in sessions:
                        dest_ws = sessions[dest]['websocket']
                        await dest_ws.send(message)
                        logger.debug(f"Forwarded {msg_type} to {dest}")
                
                else:
                    logger.warning(f"Unknown message type: {msg_type}")
            
            except json.JSONDecodeError:
                logger.error(f"Invalid JSON received from {client_id}")
            except Exception as e:
                logger.error(f"Error handling message: {e}")
    
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client disconnected: {client_id}")
    finally:
        clients.discard(websocket)
        
        # Remove user from sessions
        for user_id, session in list(sessions.items()):
            if session['websocket'] == websocket:
                del sessions[user_id]
                logger.info(f"User unregistered: {user_id}")

async def main():
    """Start WebSocket signaling server"""
    server = await websockets.serve(
        handle_client,
        "0.0.0.0",
        8088,
        ping_interval=20,
        ping_timeout=10
    )
    
    logger.info("Signaling server started on ws://0.0.0.0:8088")
    
    await server.wait_closed()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
